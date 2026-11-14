#include "board.h"

#include <Arduino.h>
#include <Wire.h>
#include <esp_heap_caps.h>
#include <Adafruit_NeoPixel.h>
#include <cst816t.h>
#define LGFX_USE_V1
#include <LovyanGFX.hpp>

#include "../app_config.h"

namespace {

// --- ST77961 kijelző (gyári konfiguráció szerint) ---
class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_ST77961 panel_;
  lgfx::Bus_SPI bus_;

public:
  LGFX() {
    {
      auto cfg = bus_.config();
      cfg.spi_host = SPI2_HOST;
      cfg.spi_mode = 0;
      cfg.freq_write = 80000000;
      cfg.freq_read = 20000000;
      cfg.spi_3wire = true;
      cfg.use_lock = true;
      cfg.dma_channel = SPI_DMA_CH_AUTO;
      cfg.pin_sclk = pins::LCD_SCLK;
      cfg.pin_mosi = pins::LCD_MOSI;
      cfg.pin_miso = -1;
      cfg.pin_dc = pins::LCD_DC;
      bus_.config(cfg);
      panel_.setBus(&bus_);
    }
    {
      auto cfg = panel_.config();
      cfg.pin_cs = pins::LCD_CS;
      cfg.pin_rst = pins::LCD_RST;
      cfg.pin_busy = -1;
      cfg.memory_width = PK_SCREEN_W;
      cfg.memory_height = PK_SCREEN_H;
      cfg.panel_width = PK_SCREEN_W;
      cfg.panel_height = PK_SCREEN_H;
      cfg.offset_x = 0;
      cfg.offset_y = 0;
      cfg.offset_rotation = 0;
      cfg.dummy_read_pixel = 8;
      cfg.dummy_read_bits = 1;
      cfg.readable = false;
      cfg.invert = false;
      cfg.rgb_order = true;
      cfg.dlen_16bit = false;
      cfg.bus_shared = false;
      panel_.config(cfg);
    }
    setPanel(&panel_);
  }
};

LGFX gfx;
cst816t touch(Wire, pins::TOUCH_RST, pins::TOUCH_INT);
Adafruit_NeoPixel ring(8, pins::RGB_LED, NEO_GRB + NEO_KHZ800);

lv_display_t *g_disp = nullptr;
QueueHandle_t g_knob_q = nullptr;

void disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px) {
  if (gfx.getStartCount() > 0) gfx.endWrite();
  gfx.pushImageDMA(area->x1, area->y1, area->x2 - area->x1 + 1, area->y2 - area->y1 + 1,
                   (lgfx::rgb565_t *)px);
  gfx.waitDMA();  // a puffert az LVGL csak a DMA vége után írhatja újra
  lv_display_flush_ready(disp);
}

struct VirtTouch {
  volatile bool active;
  int x0, y0, x1, y1;
  uint32_t start, dur;
} vt = {};

void touch_read(lv_indev_t *, lv_indev_data_t *data) {
  if (vt.active) {
    const uint32_t el = millis() - vt.start;
    if (el > vt.dur) {
      vt.active = false;
      data->state = LV_INDEV_STATE_RELEASED;
      return;
    }
    const float k = vt.dur ? (float)el / vt.dur : 1.0f;
    data->point.x = vt.x0 + (int)((vt.x1 - vt.x0) * k);
    data->point.y = vt.y0 + (int)((vt.y1 - vt.y0) * k);
    data->state = LV_INDEV_STATE_PRESSED;
    return;
  }
  if (touch.available()) {
    if (touch.x == 0 && touch.y == 0) {  // érvénytelen minta
      data->state = LV_INDEV_STATE_PRESSED;
      return;
    }
    data->point.x = touch.x;
    data->point.y = touch.y;
    data->state = LV_INDEV_STATE_PRESSED;
  } else {
    data->state = LV_INDEV_STATE_RELEASED;
  }
}

// Gyűrű és nyomógomb mintavételezése 2 ms-onként külön taskban.
// Csak eseményeket küld a sorba; LVGL-t kizárólag a loop() kezel.
void knob_task(void *) {
  int prev_a = digitalRead(pins::ENC_A);
  bool btn_down = false;
  uint32_t btn_change = 0, press_start = 0, last_release = 0;
  bool long_sent = false;
  int clicks = 0;
  constexpr uint32_t DEBOUNCE = 25, DOUBLE_WIN = 300, LONG_MS = 800;

  for (;;) {
    const uint32_t now = millis();
    // Kvadratúra: A felfutó élén B iránya dönt (gyári logika).
    const int a = digitalRead(pins::ENC_A);
    if (a != prev_a && a == HIGH) {
      KnobEvent ev = digitalRead(pins::ENC_B) != a ? KnobEvent::Cw : KnobEvent::Ccw;
      xQueueSend(g_knob_q, &ev, 0);
    }
    prev_a = a;

    const bool raw_down = digitalRead(pins::ENC_SW) == LOW;
    if (raw_down != btn_down && now - btn_change >= DEBOUNCE) {
      btn_down = raw_down;
      btn_change = now;
      if (btn_down) {
        press_start = now;
        long_sent = false;
      } else if (!long_sent) {
        clicks++;
        last_release = now;
      }
    }
    if (btn_down && !long_sent && now - press_start >= LONG_MS) {
      long_sent = true;
      clicks = 0;
      KnobEvent ev = KnobEvent::LongPress;
      xQueueSend(g_knob_q, &ev, 0);
    }
    if (clicks >= 2) {
      KnobEvent ev = KnobEvent::DoubleClick;
      xQueueSend(g_knob_q, &ev, 0);
      clicks = 0;
    } else if (clicks == 1 && !btn_down && now - last_release > DOUBLE_WIN) {
      KnobEvent ev = KnobEvent::Click;
      xQueueSend(g_knob_q, &ev, 0);
      clicks = 0;
    }
    vTaskDelay(pdMS_TO_TICKS(2));
  }
}

uint32_t lv_tick_cb() { return millis(); }

}  // namespace

void board_begin() {
  // Kijelző tápsínek, LCD reset
  pinMode(pins::LCD_PWR_1, OUTPUT);
  digitalWrite(pins::LCD_PWR_1, HIGH);
  pinMode(pins::LCD_PWR_2, OUTPUT);
  digitalWrite(pins::LCD_PWR_2, HIGH);
  pinMode(pins::LCD_RST, OUTPUT);
  digitalWrite(pins::LCD_RST, HIGH);
  delay(10);
  digitalWrite(pins::LCD_RST, LOW);
  delay(10);
  digitalWrite(pins::LCD_RST, HIGH);

  pinMode(pins::POWER_LED, OUTPUT);
  digitalWrite(pins::POWER_LED, LOW);

  // Háttérvilágítás a kép felépüléséig sötét
  ledcAttach(pins::BACKLIGHT, 5000, 8);
  ledcWrite(pins::BACKLIGHT, 0);

  Wire.setPins(pins::TOUCH_SDA, pins::TOUCH_SCL);
  Wire.begin();

  pinMode(pins::ENC_A, INPUT);
  pinMode(pins::ENC_B, INPUT);
  pinMode(pins::ENC_SW, INPUT_PULLUP);

  pinMode(pins::RGB_LED_PWR, OUTPUT);
  digitalWrite(pins::RGB_LED_PWR, HIGH);
  ring.begin();
  ring.setBrightness(20);
  ring.clear();
  ring.show();

  gfx.init();
  gfx.initDMA();
  gfx.startWrite();
  gfx.fillScreen(TFT_BLACK);

  touch.begin(mode_touch);

  lv_init();
  lv_tick_set_cb(lv_tick_cb);

  const size_t buf_size = PK_SCREEN_W * PK_SCREEN_H * 2;
  void *b1 = heap_caps_malloc(buf_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  void *b2 = heap_caps_malloc(buf_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!b1 || !b2) Serial.println("[board] HIBA: LVGL puffer foglalás sikertelen (PSRAM?)");

  g_disp = lv_display_create(PK_SCREEN_W, PK_SCREEN_H);
  lv_display_set_color_format(g_disp, LV_COLOR_FORMAT_RGB565);
  lv_display_set_flush_cb(g_disp, disp_flush);
  lv_display_set_buffers(g_disp, b1, b2, buf_size, LV_DISPLAY_RENDER_MODE_PARTIAL);

  lv_indev_t *tp = lv_indev_create();
  lv_indev_set_type(tp, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(tp, touch_read);
  lv_indev_set_display(tp, g_disp);

  g_knob_q = xQueueCreate(32, sizeof(KnobEvent));
  xTaskCreatePinnedToCore(knob_task, "knob", 3072, nullptr, 2, nullptr, 0);
}

void board_set_backlight(uint8_t pct) {
  if (pct > 100) pct = 100;
  ledcWrite(pins::BACKLIGHT, (uint32_t)pct * 255 / 100);
}

lv_display_t *board_display() { return g_disp; }

bool board_poll_knob(KnobEvent *ev) { return g_knob_q && xQueueReceive(g_knob_q, ev, 0) == pdTRUE; }

void board_inject_knob(KnobEvent ev) {
  if (g_knob_q) xQueueSend(g_knob_q, &ev, 0);
}

void board_inject_touch(int x0, int y0, int x1, int y1, uint32_t dur_ms) {
  vt.x0 = x0;
  vt.y0 = y0;
  vt.x1 = x1;
  vt.y1 = y1;
  vt.dur = dur_ms < 60 ? 60 : dur_ms;
  vt.start = millis();
  vt.active = true;
}

const char *knob_event_name(KnobEvent ev) {
  switch (ev) {
    case KnobEvent::Cw: return "CW";
    case KnobEvent::Ccw: return "CCW";
    case KnobEvent::Click: return "CLICK";
    case KnobEvent::DoubleClick: return "DOUBLE";
    case KnobEvent::LongPress: return "LONG";
    default: return "NONE";
  }
}

void board_ring_color(uint32_t rgb) {
  static uint32_t last = 0xFFFFFFFF;
  if (rgb == last) return;
  last = rgb;
  for (int i = 0; i < 8; ++i) ring.setPixelColor(i, ring.Color((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF));
  ring.show();
}
