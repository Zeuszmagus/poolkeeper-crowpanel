#!/usr/bin/env bash
# Magyar ékezetes LVGL fontok generálása (lv_font_conv 1.5.2, Montserrat OFL + FontAwesome5 szimbólumok).
# Futtatás: bash scripts/gen_fonts.sh   (a repó gyökeréből)
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
CONV="$ROOT/tools/fontconv/node_modules/.bin/lv_font_conv"
TTF="$ROOT/tools/fontconv/ttf"
SYMFONT="$ROOT/vendor/elecrow-1.46/example/V1.0/Arduino/RotaryScreen_1_46_Code_Core3_LVGL9/libraries/lvgl/scripts/built_in_font/FontAwesome5-Solid+Brands+Regular.woff"
OUT="$ROOT/firmware/poolkeeper_ui/src/fonts"
# ASCII + ° + • + – + … + → + magyar ékezetek (ÁÉÍÓÖŐÚÜŰ áéíóöőúüű)
TEXT="0x20-0x7E,0xB0,0xB7,0x2022,0x2013,0x2026,0x2192,0xC1,0xC9,0xCD,0xD3,0xD6,0x150,0xDA,0xDC,0x170,0xE1,0xE9,0xED,0xF3,0xF6,0x151,0xFA,0xFC,0x171"
SYMS="61441,61448,61451,61452,61453,61457,61459,61461,61465,61468,61473,61478,61479,61480,61502,61507,61512,61515,61516,61517,61521,61522,61523,61524,61543,61544,61550,61552,61553,61556,61559,61560,61561,61563,61587,61589,61636,61637,61639,61641,61664,61671,61674,61683,61724,61732,61787,61931,62016,62017,62018,62019,62020,62087,62099,62212,62189,62810,63426,63650"
gen() { # name size ttf withSymbols range
  local args=(--no-compress --bpp 4 --size "$2" --font "$TTF/$3" -r "$5" --format lvgl --force-fast-kern-format --lv-include lvgl.h -o "$OUT/$1.c")
  if [ "$4" = 1 ]; then args+=(--font "$SYMFONT" -r "$SYMS"); fi
  "$CONV" "${args[@]}"
  echo "  $1.c"
}
gen pk_font_16 16 Montserrat-Medium.ttf 1 "$TEXT"
gen pk_font_20 20 Montserrat-Medium.ttf 1 "$TEXT"
gen pk_font_28 28 Montserrat-Bold.ttf   1 "$TEXT"
# Nagy kijelzés: számjegyek, nagybetűk (BE/KI) és a szükséges jelek
gen pk_font_30 30 Montserrat-Bold.ttf   1 "$TEXT"
gen pk_font_36 36 Montserrat-Bold.ttf   1 "$TEXT"
gen pk_font_72 72 Montserrat-Bold.ttf   0 "0x20,0x25,0x2B-0x3A,0x41-0x5A,0xB0,0xC9,0x2013"
gen pk_font_96 96 Montserrat-Bold.ttf   0 "0x20,0x2D,0x30-0x3A"
gen pk_font_120 120 Montserrat-Bold.ttf 0 "0x20,0x2D,0x30-0x3A"
# Menüikonok: Material Design Icons 7.4.47 (Apache 2.0), npm @mdi/font
# pump, lightbulb-on, shower-head, hot-tub, waves-arrow-right, chart-bubble, shaker, chart-bar, information-outline, clock-outline
MDI="$ROOT/tools/fontconv/node_modules/@mdi/font/fonts/materialdesignicons-webfont.ttf"
"$CONV" --no-compress --bpp 4 --size 56 --font "$MDI" -r 0xF1402,0xF06E8,0xF09A1,0xF0828,0xF185A,0xF05E3,0xF110E,0xF0128,0xF02FD,0xF0150 \
  --format lvgl --lv-include lvgl.h -o "$OUT/pk_icons_56.c"
echo "  pk_icons_56.c"
"$CONV" --no-compress --bpp 4 --size 32 --font "$MDI" -r 0xF1402,0xF06E8,0xF09A1,0xF0828,0xF185A,0xF05E3,0xF110E,0xF0128,0xF02FD,0xF0150 \
  --format lvgl --lv-include lvgl.h -o "$OUT/pk_icons_32.c"
echo "  pk_icons_32.c"
