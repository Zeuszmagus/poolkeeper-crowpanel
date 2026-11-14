#pragma once
// PoolKeeper LVGL nézetréteg — mindkét profilhoz közös.
//
// Kezelés:
//  Lapozó mód   : forgatás = oldalváltás, nyomás = belépés az oldal kezelésébe,
//                 hosszú nyomás = vissza a Medence nézetre. Érintés: oldalra húzás lapoz.
//  Kezelő mód   : forgatás = kijelölés, nyomás = megerősítés, dupla nyomás = vissza.

#include "../hal/board.h"

void ui_begin();
void ui_loop();
void ui_handle_knob(KnobEvent ev);
// Profilváltás után a teljes felület újraépül (a loop()-ban, biztonságosan).
void ui_request_rebuild();
// Képernyőkép küldése a soros portra (diagnosztika).
void ui_send_screenshot();
const char *ui_page_name();
void ui_goto_page(int idx);
