#include "controller.h"

#include <Arduino.h>

#include "../model/storage.h"
#include "relay_modbus.h"

namespace {
constexpr uint32_t ACK_MS = 350;          // szimulált modul válaszideje
constexpr uint32_t FLOW_DELAY_SIM_MS = 1500;  // szimuláció: áramlás megjelenése szivattyúindítás után
constexpr uint32_t LINK_FRESH_MS = 2000;      // ennél régebbi relé-visszaolvasás már nem érvényes
bool flow_fault = false;
bool use_modbus = PK_OUT_MODBUS;
uint32_t link_lost_ms = 0;
uint32_t pump_on_since = 0;
uint32_t last_eval = 0, last_stat = 0, last_stat_save = 0;

bool out_is_on(OutState o) { return o == OutState::On || o == OutState::PendingOff; }

void set_out(int i, OutState o) {
  Channel &c = g_state.ch[i];
  if (c.out == o) return;
  c.out = o;
  Serial.printf("[out] %s: %s\n", pk_channel_name((ChannelId)i),
                o == OutState::On ? "BE" : o == OutState::Off ? "KI" : o == OutState::Error ? "HIBA" : "folyamatban");
  pk_touch();
}

// Valódi relémodul: kívánt állapot kiküldése, kijelzett állapot = visszaolvasott relé.
void apply_relays(uint32_t ms) {
  AppState &s = g_state;
  uint8_t mask = 0;
  for (int i = 0; i < CH_COUNT; ++i)
    if (PK_RELAY_OF[i] >= 0 && s.ch[i].want_on) mask |= 1 << PK_RELAY_OF[i];
  relay_set_wanted(mask);

  const RelayStatus rs = relay_status();
  const bool fresh = rs.link_ok && ms - rs.last_ok_ms < LINK_FRESH_MS;
  if (fresh) link_lost_ms = 0;
  else if (!link_lost_ms) link_lost_ms = ms ? ms : 1;
  const bool link_down = !fresh && ms - link_lost_ms >= PK_CMD_TIMEOUT_MS;
  if (s.module_ok == link_down) {
    s.module_ok = !link_down;
    Serial.printf("[relay] modul %s%s%s\n", s.module_ok ? "elérhető" : "NEM elérhető", rs.last_err[0] ? ": " : "",
                  s.module_ok ? "" : rs.last_err);
    pk_touch();
  }

  for (int i = 0; i < CH_COUNT; ++i) {
    Channel &c = s.ch[i];
    const int r = PK_RELAY_OF[i];
    if (r < 0) {  // nincs hozzá relé
      set_out(i, c.want_on ? OutState::Error : OutState::Off);
      continue;
    }
    if (!fresh) {
      // Ismeretlen tényleges állapot: rövid kiesésnél marad a kijelzés, tartós hibánál HIBA (ha be kellene/lehet kapcsolva)
      if (link_down && (c.want_on || out_is_on(c.out) || c.out == OutState::PendingOn)) set_out(i, OutState::Error);
      continue;
    }
    const bool on = (rs.actual >> r) & 1;
    if (on == c.want_on) {
      set_out(i, on ? OutState::On : OutState::Off);
    } else {
      const OutState pend = c.want_on ? OutState::PendingOn : OutState::PendingOff;
      if (c.out != pend && c.out != OutState::Error) {
        c.cmd_sent_ms = ms;
        set_out(i, pend);
      } else if (c.out == pend && ms - c.cmd_sent_ms >= PK_CMD_TIMEOUT_MS) {
        set_out(i, OutState::Error);  // a modul válaszol, de a relé nem követi a parancsot
      } else if (c.out == OutState::Error && ms - c.cmd_sent_ms >= 3 * PK_CMD_TIMEOUT_MS) {
        c.cmd_sent_ms = ms;  // időnként újrapróbáljuk
        set_out(i, pend);
      }
    }
  }
}

void evaluate() {
  AppState &s = g_state;
  const int now = pk_now_min();
  const uint32_t ms = millis();

  // Keringés (áramlás) visszajelzés szimulációja: a szivattyú visszaigazolt BE állapotát követi
  const bool pump_on = s.ch[CH_PUMP].out == OutState::On;
  if (pump_on && !pump_on_since) pump_on_since = ms;
  if (!pump_on) pump_on_since = 0;
  // Áramlásérzékelő nincs: a keringés igazolt, ha a szivattyú visszaigazoltan BE van legalább PK_FLOW_CONFIRM_MS ideje.
  const uint32_t flow_delay = use_modbus ? PK_FLOW_CONFIRM_MS : FLOW_DELAY_SIM_MS;
  const bool flow = pump_on && !flow_fault && ms - pump_on_since >= flow_delay;
  if (flow != s.flow_ok) {
    s.flow_ok = flow;
    pk_touch();
  }

  for (int i = 0; i < CH_COUNT; ++i) {
    Channel &c = s.ch[i];
    bool request;
    bool running = false;
    if (i == CH_SALT) {
      running = now >= 0 && pk_salt_window_active(now);
      request = c.mode == Mode::Manual || (c.mode == Mode::Auto && running);
      // A sóbontó csak igazolt keringés mellett működhet; keringésvesztéskor a kérés visszavonva.
      const bool waiting = request && !s.flow_ok;
      if (waiting != c.waiting_flow) {
        c.waiting_flow = waiting;
        pk_touch();
      }
      if (waiting) request = false;
    } else {
      running = now >= 0 && pk_program_active(c.program, now);
      // Érvénytelen óránál az automata időzítés felfüggesztve.
      request = c.mode == Mode::Manual || (c.mode == Mode::Auto && running);
    }
    if (running != c.program_running) {
      c.program_running = running;
      pk_touch();
    }
    if (request != c.want_on) {
      c.want_on = request;
      pk_touch();
    }

    if (use_modbus) continue;  // a relémodul kimeneteit a ciklus után, egyben kezeljük
    // Kimeneti modul: parancs küldése, visszaigazolás vagy hiba
    const bool confirmed_on = out_is_on(c.out);
    const bool pending = c.out == OutState::PendingOn || c.out == OutState::PendingOff;
    if (!pending && (c.out == OutState::Error ? true : confirmed_on != c.want_on)) {
      if (c.out == OutState::Error && !s.module_ok) continue;  // hibánál csak modul visszatérésekor próbáljuk újra
      c.out = c.want_on ? OutState::PendingOn : OutState::PendingOff;
      c.cmd_sent_ms = ms;
      Serial.printf("[out] %s -> %s\n", pk_channel_name((ChannelId)i), c.want_on ? "BE" : "KI");
      pk_touch();
    } else if (pending) {
      if (s.module_ok && ms - c.cmd_sent_ms >= ACK_MS) {
        c.out = c.out == OutState::PendingOn ? OutState::On : OutState::Off;
        Serial.printf("[out] %s visszaigazolva: %s\n", pk_channel_name((ChannelId)i), c.out == OutState::On ? "BE" : "KI");
        pk_touch();
      } else if (!s.module_ok && ms - c.cmd_sent_ms >= PK_CMD_TIMEOUT_MS) {
        c.out = OutState::Error;
        Serial.printf("[out] %s HIBA: nincs visszaigazolás\n", pk_channel_name((ChannelId)i));
        pk_touch();
      }
    }
  }
  if (use_modbus) apply_relays(ms);
}

void update_stats() {
  AppState &s = g_state;
  struct tm t;
  const uint32_t ms = millis();
  const uint32_t dt = (ms - last_stat) / 1000;
  if (dt == 0) return;
  last_stat += dt * 1000;
  if (pk_clock_now(&t)) {
    const int key = (t.tm_year + 1900) * 10000 + (t.tm_mon + 1) * 100 + t.tm_mday;
    if (s.stats.day_key && key != s.stats.day_key) {
      s.stats.yesterday_s = s.stats.today_s;
      s.stats.today_s = 0;
    }
    s.stats.day_key = key;
  }
  if (s.ch[CH_PUMP].out == OutState::On) {
    s.stats.today_s += dt;
    s.stats.total_s += dt;
  }
  if (ms - last_stat_save >= 10 * 60 * 1000UL) {
    last_stat_save = ms;
    storage_save_stats();
  }
}
}  // namespace

void controller_begin() {
  AppState &s = g_state;
  s.module_ok = true;
  // Induláskor minden kimenet kikapcsolt állapotból indul. Az üzemmódok az áramszünet előtti állapotra
  // állnak vissza: AUTO → AUTO (az időprogram fut tovább, amint érvényes az idő), KÉZI → 0 (biztonság).
  Mode saved[CH_COUNT];
  const bool have = storage_load_modes(saved);
  for (int i = 0; i < CH_COUNT; ++i) {
    Mode m = have ? saved[i] : Mode::Off;
    if (m == Mode::Manual && !PK_RESTORE_MANUAL) m = Mode::Off;
    s.ch[i].mode = m;
    s.ch[i].out = OutState::Off;
    s.ch[i].want_on = false;
  }
  if (have) Serial.println("[mode] üzemmódok visszaállítva (AUTO marad, KÉZI → 0)");
  if (use_modbus) {
    for (int i = 0; i < CH_COUNT; ++i)
      if (PK_RELAY_OF[i] < 0) s.layout.visible[i] = false;  // relé nélküli eszköz rejtve
    relay_begin();
  }
  last_stat = last_stat_save = millis();
  pk_touch();
}

void controller_loop() {
  const uint32_t ms = millis();
  if (ms - last_eval >= 100) {
    last_eval = ms;
    evaluate();
    update_stats();
  }
}

void controller_set_mode(ChannelId id, Mode m) {
  if (id >= CH_COUNT || g_state.ch[id].mode == m) return;
  g_state.ch[id].mode = m;
  storage_save_modes();  // áramszünet után ebből áll vissza
  Serial.printf("[mode] %s = %s\n", pk_channel_name(id), pk_mode_name(m));
  evaluate();  // azonnali kapcsolás
  pk_touch();
}

bool controller_save_program(ChannelId id, const Program &in) {
  if (id >= CH_COUNT || in.count > PK_MAX_PERIODS) return false;
  Program p = in;
  for (int i = 0; i < p.count; ++i) {
    if (p.p[i].off_min <= p.p[i].on_min || p.p[i].off_min > 1439) return false;
    if (p.p[i].id == 0) p.p[i].id = g_state.next_period_id++;
  }
  pk_program_sort(p);
  g_state.ch[id].program = p;
  storage_save_program(id);
  if (id == CH_PUMP) {
    pk_salt_revalidate();
    storage_save_salt();
  }
  Serial.printf("[prog] %s mentve: %d időszak\n", pk_channel_name(id), p.count);
  evaluate();
  pk_touch();
  return true;
}

void controller_save_salt(const SaltConfig &c) {
  g_state.salt = c;
  g_state.salt_needs_check = false;
  for (int i = 0; i < c.count; ++i)
    if (c.c[i].invalid) g_state.salt_needs_check = true;
  storage_save_salt();
  evaluate();
  pk_touch();
}

void controller_use_modbus(bool on) {
  if (on == use_modbus) return;
  use_modbus = on;
  for (int i = 0; i < CH_COUNT; ++i) g_state.ch[i].out = OutState::Off;
  if (on) relay_begin();
  else relay_set_wanted(0);
  g_state.module_ok = true;
  link_lost_ms = 0;
  Serial.printf("[out] kimenet: %s\n", on ? "Modbus relémodul" : "szimuláció");
  pk_touch();
}
bool controller_use_modbus() { return use_modbus; }

void controller_sim_module(bool ok) {
  if (use_modbus) return;
  g_state.module_ok = ok;
  pk_touch();
}
void controller_sim_flow_fault(bool fault) {
  flow_fault = fault;
  pk_touch();
}
bool controller_sim_flow_fault() { return flow_fault; }
