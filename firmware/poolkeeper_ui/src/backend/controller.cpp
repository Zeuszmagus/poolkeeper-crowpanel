#include "controller.h"

#include <Arduino.h>

#include "../model/storage.h"

namespace {
constexpr uint32_t ACK_MS = 350;          // szimulált modul válaszideje
constexpr uint32_t FLOW_DELAY_MS = 1500;  // áramlás megjelenése szivattyúindítás után
bool flow_fault = false;
uint32_t pump_on_since = 0;
uint32_t last_eval = 0, last_stat = 0, last_stat_save = 0;

bool out_is_on(OutState o) { return o == OutState::On || o == OutState::PendingOff; }

void evaluate() {
  AppState &s = g_state;
  const int now = pk_now_min();
  const uint32_t ms = millis();

  // Keringés (áramlás) visszajelzés szimulációja: a szivattyú visszaigazolt BE állapotát követi
  const bool pump_on = s.ch[CH_PUMP].out == OutState::On;
  if (pump_on && !pump_on_since) pump_on_since = ms;
  if (!pump_on) pump_on_since = 0;
  const bool flow = pump_on && !flow_fault && ms - pump_on_since >= FLOW_DELAY_MS;
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

void controller_sim_module(bool ok) {
  g_state.module_ok = ok;
  pk_touch();
}
void controller_sim_flow_fault(bool fault) {
  flow_fault = fault;
  pk_touch();
}
bool controller_sim_flow_fault() { return flow_fault; }
