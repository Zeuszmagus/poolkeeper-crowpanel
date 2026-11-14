// PoolKeeper CrowPanel 1.46" — ügyfélfelület (PoolKeeper_CrowPanel_UI_terv.md alapján)
// Hardver: Elecrow CrowPanel 1.46" (DHR55146D), ESP32-S3R8, 16 MB flash, 8 MB OPI PSRAM
// Platform: Arduino ESP32 core 3.3.8, LVGL 9.1.0, LovyanGFX 1.2.7

#include <Arduino.h>
#include <time.h>

#include "src/app_config.h"
#include "src/backend/controller.h"
#include "src/diag/console.h"
#include "src/hal/board.h"
#include "src/model/model.h"
#include "src/model/storage.h"
#include "src/net/net.h"
#include "src/ui/ui.h"

void setup() {
  Serial.setTxBufferSize(16384);
  Serial.begin(115200);
  Serial.setTxTimeoutMs(500);
  const uint32_t t0 = millis();
  while (!Serial && millis() - t0 < 800) delay(1);
  Serial.printf("\nPoolKeeper UI %s  (Arduino-ESP32 %s, LVGL %d.%d.%d)\n", PK_FW_VERSION, ESP_ARDUINO_VERSION_STR,
                LVGL_VERSION_MAJOR, LVGL_VERSION_MINOR, LVGL_VERSION_PATCH);

  setenv("TZ", PK_TZ, 1);
  tzset();
  // Az óra csak akkor érvényes, ha már be volt állítva (szoftveres újraindítás megtartja; áramtalanítás nem).
  // Áramszünet után a Wi-Fi-n keresztül (NTP) magától beáll.
  g_state.clock_valid = time(nullptr) > 1704067200;  // 2024-01-01

  storage_begin();
  controller_begin();
  board_begin();
  ui_begin();
  lv_timer_handler();
  net_begin();
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
  controller_loop();
  net_loop();
  ota_validate_loop();
  ui_loop();
  lv_timer_handler();
  delay(5);
}
