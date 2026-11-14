// PoolKeeper CrowPanel 1.46" kijelző-firmware
// Hardver: Elecrow CrowPanel 1.46" (DHR55146D), ESP32-S3R8, 16 MB flash, 8 MB OPI PSRAM
// Platform: Arduino ESP32 core 3.3.8, LVGL 9.1.0, LovyanGFX 1.2.7
//
// M1: közös magyar UI szimulált vezérlővel (Basic és Full profil).

#include <Arduino.h>
#include <sys/time.h>
#include <time.h>

#include "src/app_config.h"
#include "src/backend/backend.h"
#include "src/diag/console.h"
#include "src/hal/board.h"
#include "src/model/model.h"
#include "src/ui/ui.h"

// A fordítás idejéből szimulált óra (M1). M2: NTP és/vagy elemes RTC.
static void init_sim_clock() {
  setenv("TZ", PK_TZ, 1);
  tzset();
  static const char *months = "JanFebMarAprMayJunJulAugSepOctNovDec";
  char mon[4] = {0};
  struct tm t = {};
  int day, year, h, m, s;
  sscanf(__DATE__, "%3s %d %d", mon, &day, &year);
  sscanf(__TIME__, "%d:%d:%d", &h, &m, &s);
  t.tm_mon = (strstr(months, mon) - months) / 3;
  t.tm_mday = day;
  t.tm_year = year - 1900;
  t.tm_hour = h;
  t.tm_min = m;
  t.tm_sec = s;
  t.tm_isdst = -1;
  struct timeval tv = {mktime(&t), 0};
  settimeofday(&tv, nullptr);
  g_state.clock_valid = true;
  g_state.clock_simulated = true;
}

void setup() {
  Serial.setTxBufferSize(16384);  // képernyőkép-átvitelhez
  Serial.begin(115200);
  Serial.setTxTimeoutMs(500);
  const uint32_t t0 = millis();
  while (!Serial && millis() - t0 < 800) delay(1);

  Serial.printf("\nPoolKeeper panel %s  (Arduino-ESP32 %s, LVGL %d.%d.%d)\n", PK_FW_VERSION, ESP_ARDUINO_VERSION_STR,
                LVGL_VERSION_MAJOR, LVGL_VERSION_MINOR, LVGL_VERSION_PATCH);
  Serial.printf("PSRAM: %u bájt, flash: %u bájt\n", (unsigned)ESP.getPsramSize(), (unsigned)ESP.getFlashChipSize());

  init_sim_clock();
  board_begin();
  backend_switch_profile(Profile::Basic);
  ui_begin();
  lv_timer_handler();
  board_set_backlight(PK_BACKLIGHT_PCT);
  Serial.println("READY  (írd be: help)");
}

void loop() {
  KnobEvent ev;
  while (board_poll_knob(&ev)) {
    Serial.printf("[knob] %s\n", knob_event_name(ev));
    ui_handle_knob(ev);
  }
  console_poll();
  backend().loop();
  ui_loop();
  lv_timer_handler();
  delay(5);
}
