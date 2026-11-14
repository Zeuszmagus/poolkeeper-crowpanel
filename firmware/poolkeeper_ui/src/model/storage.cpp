#include "storage.h"

#include <Arduino.h>
#include <Preferences.h>
#include <string.h>

namespace {
constexpr uint32_t SCHEMA = 1;
Preferences prefs;

void defaults_layout(Layout &l) {
  for (int i = 0; i < CH_COUNT; ++i) {
    l.order[i] = (uint8_t)i;
    l.visible[i] = true;
  }
}

bool layout_valid(const Layout &l) {
  bool seen[CH_COUNT] = {};
  for (int i = 0; i < CH_COUNT; ++i) {
    if (l.order[i] >= CH_COUNT || seen[l.order[i]]) return false;
    seen[l.order[i]] = true;
  }
  return true;
}

bool program_valid(const Program &p) {
  if (p.count > PK_MAX_PERIODS) return false;
  for (int i = 0; i < p.count; ++i)
    if (p.p[i].off_min > 1439 || p.p[i].off_min <= p.p[i].on_min || p.p[i].id == 0) return false;
  return true;
}

void key_prog(char *k, int id) { snprintf(k, 8, "prog%d", id); }
}  // namespace

void storage_begin() {
  AppState &s = g_state;
  defaults_layout(s.layout);
  s.face = ClockFace::Analog;
  s.next_period_id = 1;
  for (int i = 0; i < CH_COUNT; ++i) s.ch[i].program = {};
  s.salt = {};

  prefs.begin("pk", false);
  if (prefs.getUInt("schema", 0) != SCHEMA) {
    Serial.println("[nvs] új / eltérő séma -> alapértékek");
    prefs.clear();
    prefs.putUInt("schema", SCHEMA);
    return;
  }
  s.face = (ClockFace)prefs.getUChar("face", 0);
  s.next_period_id = prefs.getUShort("pid", 1);
  char k[8];
  for (int i = 0; i < CH_COUNT; ++i) {
    key_prog(k, i);
    Program p = {};
    if (prefs.getBytes(k, &p, sizeof p) == sizeof p && program_valid(p)) s.ch[i].program = p;
  }
  SaltConfig sc = {};
  if (prefs.getBytes("salt", &sc, sizeof sc) == sizeof sc && sc.count <= PK_MAX_PERIODS) s.salt = sc;
  s.salt_needs_check = prefs.getBool("saltchk", false);
  Layout l;
  if (prefs.getBytes("layout", &l, sizeof l) == sizeof l && layout_valid(l)) s.layout = l;
  s.stats.total_s = prefs.getUInt("tot", 0);
  Serial.println("[nvs] beállítások betöltve");
}

void storage_save_face() { prefs.putUChar("face", (uint8_t)g_state.face); }

void storage_save_program(ChannelId id) {
  char k[8];
  key_prog(k, id);
  prefs.putBytes(k, &g_state.ch[id].program, sizeof(Program));
  prefs.putUShort("pid", g_state.next_period_id);
}

void storage_save_salt() {
  prefs.putBytes("salt", &g_state.salt, sizeof(SaltConfig));
  prefs.putBool("saltchk", g_state.salt_needs_check);
}

void storage_save_layout() { prefs.putBytes("layout", &g_state.layout, sizeof(Layout)); }

void storage_save_stats() { prefs.putUInt("tot", g_state.stats.total_s); }

void storage_save_modes() {
  uint8_t m[CH_COUNT];
  for (int i = 0; i < CH_COUNT; ++i) m[i] = (uint8_t)g_state.ch[i].mode;
  prefs.putBytes("modes", m, sizeof m);
}

bool storage_load_modes(Mode out[CH_COUNT]) {
  uint8_t m[CH_COUNT];
  if (prefs.getBytes("modes", m, sizeof m) != sizeof m) return false;
  for (int i = 0; i < CH_COUNT; ++i) out[i] = m[i] <= 2 ? (Mode)m[i] : Mode::Off;
  return true;
}

void storage_factory_reset() {
  prefs.clear();
  prefs.putUInt("schema", SCHEMA);
}
