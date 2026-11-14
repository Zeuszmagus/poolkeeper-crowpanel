#pragma once
#include <lvgl.h>

LV_FONT_DECLARE(pk_font_16);
LV_FONT_DECLARE(pk_font_20);
LV_FONT_DECLARE(pk_font_28);
LV_FONT_DECLARE(pk_font_72);

namespace col {
constexpr uint32_t BG = 0x000000;
constexpr uint32_t SURFACE = 0x0E1A1F;
constexpr uint32_t TRACK = 0x1B2B31;
constexpr uint32_t TEAL = 0x1FD1C1;
constexpr uint32_t TEAL_DIM = 0x0F5F59;
constexpr uint32_t TEXT = 0xFFFFFF;
constexpr uint32_t MUTED = 0x8FA3AB;
constexpr uint32_t STALE = 0x5C6A70;
constexpr uint32_t OK = 0x3DDC84;
constexpr uint32_t PENDING = 0xFFD54A;
constexpr uint32_t WARN = 0xFFA23A;
constexpr uint32_t ERR = 0xFF5A5A;
}  // namespace col

inline lv_color_t C(uint32_t hex) { return lv_color_hex(hex); }
