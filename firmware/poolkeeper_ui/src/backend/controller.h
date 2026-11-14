#pragma once
// Vezérlési logika (Basic: a panel saját ütemezője) és a kimeneti modul szimulációja.
// A nézetréteg csak ezen keresztül változtat: üzemmód, program mentése, sóbontó-beállítás.
// A kijelző a kimenet állapotát a modul visszaigazolása alapján mutatja (Channel::out).

#include "../model/model.h"

void controller_begin();
void controller_loop();

void controller_set_mode(ChannelId id, Mode m);
// Program mentése: új időszakok stabil azonosítót kapnak, rendezés, NVS. Hamis, ha érvénytelen.
bool controller_save_program(ChannelId id, const Program &p);
void controller_save_salt(const SaltConfig &c);

// Szimuláció / diagnosztika
void controller_sim_module(bool ok);
void controller_sim_flow_fault(bool fault);
bool controller_sim_flow_fault();
