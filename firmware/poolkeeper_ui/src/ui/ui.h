#pragma once
// PoolKeeper ügyfélfelület — publikus belépési pontok.
#include "../hal/board.h"

void ui_begin();
void ui_loop();
void ui_handle_knob(KnobEvent ev);
void ui_send_screenshot();
const char *ui_view_name();
