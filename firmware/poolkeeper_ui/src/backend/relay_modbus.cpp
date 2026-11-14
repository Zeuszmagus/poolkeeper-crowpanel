#include "relay_modbus.h"

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <string.h>

#include "../app_config.h"

namespace {
HardwareSerial &bus = Serial0;  // UART0 (GPIO43/44, J34); a konzol a natív USB-n fut
SemaphoreHandle_t bus_lock = nullptr;
portMUX_TYPE st_mux = portMUX_INITIALIZER_UNLOCKED;
RelayStatus st = {};
volatile uint8_t wanted = 0;

constexpr uint32_t RESP_TIMEOUT_MS = 200;
constexpr uint32_t CYCLE_MS = 250;

uint16_t crc16(const uint8_t *d, size_t n) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < n; ++i) {
    crc ^= d[i];
    for (int b = 0; b < 8; ++b) crc = (crc & 1) ? (crc >> 1) ^ 0xA001 : crc >> 1;
  }
  return crc;
}

void set_err(const char *e) {
  portENTER_CRITICAL(&st_mux);
  st.link_ok = false;
  st.err_count++;
  strlcpy(st.last_err, e, sizeof st.last_err);
  portEXIT_CRITICAL(&st_mux);
}

// Egy kérés–válasz tranzakció. req: CRC nélkül; resp: a teljes válasz (CRC-vel), expect = várt hossz.
// Visszatérés: 0 siker, <0 hiba (az okát set_err rögzíti).
int transact(uint8_t *req, size_t req_len, uint8_t *resp, size_t expect) {
  const uint16_t c = crc16(req, req_len);
  req[req_len] = c & 0xFF;
  req[req_len + 1] = c >> 8;
  xSemaphoreTake(bus_lock, portMAX_DELAY);
  while (bus.available()) bus.read();  // elavult bájtok (pl. indulási ROM-napló visszhangja)
  bus.write(req, req_len + 2);
  bus.flush();
  size_t got = 0;
  const uint32_t t0 = millis();
  while (got < expect && millis() - t0 < RESP_TIMEOUT_MS) {
    if (bus.available()) {
      resp[got++] = (uint8_t)bus.read();
      // Kivétel-válasz (funkciókód | 0x80) 5 bájtos
      if (got == 2 && (resp[1] & 0x80)) expect = 5;
    } else {
      vTaskDelay(1);
    }
  }
  delay(5);  // Modbus keretköz (3,5 karakter 9600 baudon ≈ 4 ms)
  xSemaphoreGive(bus_lock);

  if (got == 0) return set_err("nincs válasz"), -1;
  if (got < expect) return set_err("csonka válasz"), -2;
  const uint16_t rc = crc16(resp, expect - 2);
  if (resp[expect - 2] != (rc & 0xFF) || resp[expect - 1] != (rc >> 8)) return set_err("CRC hiba"), -3;
  if (resp[0] != req[0]) return set_err("idegen cím"), -4;
  if (resp[1] & 0x80) {
    char b[40];
    snprintf(b, sizeof b, "Modbus kivétel %u", resp[2]);
    set_err(b);
    return -5;
  }
  return 0;
}

// FC 0x0F: mind a 4 relé egyszerre
bool write_mask(uint8_t mask) {
  uint8_t req[12] = {PK_RELAY_ADDR, 0x0F, 0x00, 0x00, 0x00, 0x04, 0x01, (uint8_t)(mask & 0x0F)};
  uint8_t resp[8];
  return transact(req, 8, resp, 8) == 0;
}

// FC 0x01: relék állapota
bool read_mask(uint8_t &mask) {
  uint8_t req[8] = {PK_RELAY_ADDR, 0x01, 0x00, 0x00, 0x00, 0x04};
  uint8_t resp[6];
  if (transact(req, 6, resp, 6) != 0) return false;
  if (resp[1] != 0x01 || resp[2] != 1) return set_err("hibás állapotválasz"), false;
  mask = resp[3] & 0x0F;
  return true;
}

void task(void *) {
  uint8_t last_actual = 0xFF;
  for (;;) {
    const uint8_t w = wanted;
    uint8_t actual;
    bool ok = read_mask(actual);
    if (ok && actual != w) {
      Serial.printf("[relay] írás: %X (most %X)\n", w, actual);
      ok = write_mask(w) && read_mask(actual);
    }
    if (ok) {
      portENTER_CRITICAL(&st_mux);
      if (!st.link_ok) st.last_err[0] = 0;
      st.link_ok = true;
      st.ever_ok = true;
      st.actual = actual;
      st.last_ok_ms = millis();
      st.ok_count++;
      portEXIT_CRITICAL(&st_mux);
      if (actual != last_actual) {
        Serial.printf("[relay] állapot: %X\n", actual);
        last_actual = actual;
      }
    }
    vTaskDelay(pdMS_TO_TICKS(CYCLE_MS));
  }
}
}  // namespace

void relay_begin() {
  if (bus_lock) return;
  bus_lock = xSemaphoreCreateMutex();
  bus.begin(PK_RS485_BAUD, SERIAL_8N1, PK_RS485_RX, PK_RS485_TX);
  wanted = 0;
  write_mask(0);  // induláskor minden relé KI, függetlenül attól, mit őrzött a modul
  xTaskCreatePinnedToCore(task, "relay", 4096, nullptr, 2, nullptr, 0);
  Serial.printf("[relay] Modbus RTU: cím %u, %lu baud, TX %d RX %d\n", PK_RELAY_ADDR, (unsigned long)PK_RS485_BAUD,
                PK_RS485_TX, PK_RS485_RX);
}

void relay_set_wanted(uint8_t mask) { wanted = mask & 0x0F; }

RelayStatus relay_status() {
  portENTER_CRITICAL(&st_mux);
  RelayStatus s = st;
  portEXIT_CRITICAL(&st_mux);
  return s;
}

int relay_read_version() {
  if (!bus_lock) return -100;
  uint8_t req[8] = {PK_RELAY_ADDR, 0x03, 0x80, 0x00, 0x00, 0x01};
  uint8_t resp[7];
  if (transact(req, 6, resp, 7) != 0) return -1;
  return (resp[3] << 8) | resp[4];
}
