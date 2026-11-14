#pragma once
// Tartós beállítások NVS-ben (Preferences, "pk" névtér, verziózott séma).
// Mentve: számlap, csatornánkénti napi program, sóbontó-ciklusok, telepítői elrendezés, összes üzemidő.
// Üzemmódok: mentve; induláskor az AUTO visszaáll, a KÉZI biztonsági okból 0 lesz (PK_RESTORE_MANUAL).

#include "model.h"

void storage_begin();      // betöltés (vagy alapértékek, ha nincs / más a séma)
void storage_save_face();
void storage_save_program(ChannelId id);
void storage_save_salt();
void storage_save_layout();
void storage_save_stats();
void storage_save_modes();
bool storage_load_modes(Mode out[CH_COUNT]);
void storage_factory_reset();
