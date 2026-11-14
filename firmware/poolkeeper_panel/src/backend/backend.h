#pragma once
// Vezérlési háttér interfész. A nézetréteg csak ezen keresztül kér műveletet;
// a döntést (és a visszaigazolást) mindig a háttér adja.
//  - Basic: helyi ütemező + külső kimeneti modul (M2-ben valós)
//  - Full:  Raspberry Pi vezérlő helyi MQTT-n (M3-ban valós)
// M1-ben mindkét profilt a SimBackend szimulálja.

#include "../model/model.h"

class Backend {
public:
  virtual ~Backend() = default;
  virtual void begin(Profile p) = 0;
  virtual void loop() = 0;
  // Konkrét célállapot kérése (nem "toggle").
  virtual void request_output(int idx, bool target_on) = 0;
  // Kézi felülbírálás törlése, visszatérés automata módba.
  virtual void request_auto(int idx) = 0;
  virtual bool save_schedule(int idx, const Schedule &s) = 0;

  // Szimulációs kapcsolók (diagnosztika / demó)
  virtual void sim_set_link(bool ok) = 0;
  virtual void sim_set_sensor_fault(bool fault) = 0;
  virtual void sim_set_clock_valid(bool valid) = 0;
  virtual bool sim_sensor_fault() const = 0;
};

Backend &backend();
void backend_switch_profile(Profile p);
