#pragma once
// CrowPanel 1.46" (DHR55146D, ESP32-S3R8) hardverréteg.
// Lábkiosztás a gyári Elecrow RotaryScreen_1_46 forrás és README alapján.

#include <stdint.h>
#include <lvgl.h>

namespace pins {
constexpr int LCD_SCLK = 10, LCD_MOSI = 11, LCD_CS = 9, LCD_DC = 3, LCD_RST = 14;
constexpr int TOUCH_SDA = 6, TOUCH_SCL = 7, TOUCH_RST = 13, TOUCH_INT = 5;
constexpr int BACKLIGHT = 46;
constexpr int ENC_A = 45, ENC_B = 42, ENC_SW = 41;
constexpr int POWER_LED = 40;      // aktív alacsony
constexpr int RGB_LED = 48;        // 8 db WS2812 gyűrű
constexpr int RGB_LED_PWR = 17;
constexpr int LCD_PWR_1 = 1, LCD_PWR_2 = 2;  // kijelző tápsínek engedélyezése
constexpr int AUX_I2C_SDA = 38, AUX_I2C_SCL = 39;
}  // namespace pins

// Kijelző, érintés, háttérvilágítás és LVGL indítása.
void board_begin();
void board_set_backlight(uint8_t pct);
lv_display_t *board_display();

// Gyűrű / gomb események (a hardvertask tölti, a loop() üríti).
enum class KnobEvent : uint8_t { None, Cw, Ccw, Click, DoubleClick, LongPress };
bool board_poll_knob(KnobEvent *ev);
// Szimulált esemény beküldése (soros diagnosztika).
void board_inject_knob(KnobEvent ev);
const char *knob_event_name(KnobEvent ev);

// Virtuális érintés (soros diagnosztika): (x0,y0)→(x1,y1) húzás dur_ms alatt; azonos pont = koppintás.
void board_inject_touch(int x0, int y0, int x1, int y1, uint32_t dur_ms);

// LED-gyűrű egyszínű állapotjelzés (0xRRGGBB, 0 = ki)
void board_ring_color(uint32_t rgb);
