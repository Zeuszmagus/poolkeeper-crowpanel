/**
 * PoolKeeper CrowPanel 1.46" — LVGL 9.1 konfiguráció.
 * A gyári Elecrow lv_conf.h-ra épül (RGB565, OS nélkül), PoolKeeper bővítésekkel.
 * Ennek a fájlnak a "lvgl" könyvtármappa mellett kell lennie.
 */
#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

/* Kijelző: ST77961, RGB565 */
#define LV_COLOR_DEPTH 16

/* Egyszálú használat: az LVGL-t kizárólag a loop() hívja. */
#define LV_USE_OS LV_OS_NONE

/* A libc malloc-ot használjuk: az ESP32-S3-on a nagy foglalások PSRAM-ba kerülnek. */
#define LV_USE_STDLIB_MALLOC LV_STDLIB_CLIB
#define LV_USE_STDLIB_STRING LV_STDLIB_CLIB
#define LV_USE_STDLIB_SPRINTF LV_STDLIB_CLIB

#define LV_DEF_REFR_PERIOD 20

/* Widgetek */
#define LV_USE_ARC 1
#define LV_USE_BAR 1
#define LV_USE_BUTTON 1
#define LV_USE_LABEL 1
#define LV_USE_IMAGE 1
#define LV_USE_LINE 1
#define LV_USE_ROLLER 1
#define LV_USE_SWITCH 1
#define LV_USE_SPINNER 1

/* Beépített fontok: csak ASCII. Magyar szöveghez a src/fonts/pk_font_*.c fontokat használjuk. */
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_DEFAULT &lv_font_montserrat_14

/* UTF-8 szövegkezelés (ékezetek) */
#define LV_TXT_ENC LV_TXT_ENC_UTF8

/* Képernyőkép a soros diagnosztikához */
#define LV_USE_SNAPSHOT 1

/* QR-kód (Wi-Fi beállító portálhoz) */
#define LV_USE_QRCODE 1

#define LV_USE_THEME_DEFAULT 1
#define LV_THEME_DEFAULT_DARK 1
#define LV_USE_FLEX 1
#define LV_USE_GRID 1

#define LV_BUILD_EXAMPLES 0
#define LV_USE_DEMO_WIDGETS 0
#define LV_USE_DEMO_KEYPAD_AND_ENCODER 0
#define LV_USE_DEMO_BENCHMARK 0
#define LV_USE_DEMO_RENDER 0
#define LV_USE_DEMO_STRESS 0
#define LV_USE_DEMO_MUSIC 0

#define LV_USE_LOG 0
#define LV_USE_ASSERT_NULL 1
#define LV_USE_ASSERT_MALLOC 1

#endif /* LV_CONF_H */
