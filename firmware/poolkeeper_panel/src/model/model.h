#pragma once
// PoolKeeper panel adatmodell — profilfüggetlen állapot, amit a nézetréteg megjelenít.

#include <stdint.h>
#include <time.h>

enum class Profile : uint8_t { Basic = 0, Full = 1 };

// Parancs életciklusa: kért → (visszaigazolt | hiba)
enum class CmdState : uint8_t { Idle, Pending, Confirmed, Failed };

enum class OutputMode : uint8_t { Auto, Manual };

static constexpr int PK_MAX_OUTPUTS = 6;

struct Output {
  const char *key;          // stabil azonosító a protokollhoz (pl. "pump")
  const char *name;         // magyar megjelenítési név
  bool on;                  // vezérlő által visszaigazolt állapot
  bool has_schedule;        // időzíthető-e
  // Visszajelző bemenet: fizikai működést csak akkor írunk ki, ha van.
  bool has_feedback;
  bool feedback_ok;         // pl. áramláskapcsoló jelez
  const char *feedback_label;
  OutputMode mode;
  time_t override_until;    // kézi felülbírálás lejárata (epoch), 0 = nincs
  // Folyamatban lévő / utolsó parancs
  CmdState cmd;
  bool cmd_target;          // kért célállapot (BE/KI)
  uint32_t cmd_id;
  uint32_t cmd_sent_ms;
  uint32_t cmd_done_ms;
  char cmd_error[48];
};

struct Reading {
  bool present;             // van-e egyáltalán érzékelő/adat
  float value;
  uint32_t updated_ms;      // utolsó friss adat ideje (millis), 0 = még soha
};

// Heti időablak. start > end: éjfélen átnyúló ablak.
struct Schedule {
  bool enabled;
  uint8_t days;             // bit0 = hétfő ... bit6 = vasárnap
  uint16_t start_min;       // 0..1439
  uint16_t end_min;         // 0..1439
};

struct AppState {
  Profile profile;
  bool link_ok;             // Full: Pi kapcsolat; Basic: kimeneti modul
  bool clock_valid;
  bool clock_simulated;
  uint32_t link_lost_ms;
  int output_count;
  Output outputs[PK_MAX_OUTPUTS];
  Schedule schedules[PK_MAX_OUTPUTS];
  Reading water_temp, ph, orp;
  uint32_t state_rev;       // minden változásnál nő (UI frissítéshez)
};

extern AppState g_state;

// --- Időzítés logika (tiszta függvények, tesztelhetők) ---

// Hét napja hétfő=0 ... vasárnap=6 a struct tm alapján.
int pk_weekday_mon0(const struct tm &t);

// Aktív-e az ablak a megadott helyi időpontban. Kezeli az éjfélen átnyúló ablakot:
// ilyenkor az éjfél utáni rész az előző nap kijelöléséhez tartozik.
bool pk_schedule_active(const Schedule &s, const struct tm &local);

// Következő állapotváltás keresése legfeljebb 8 napra előre, percenkénti léptetéssel
// epoch időben (így a nyári/téli óraátállást a localtime_r kezeli).
// Visszatérés: true, ha talált; *when = váltás ideje, *to_on = új állapot.
bool pk_schedule_next_change(const Schedule &s, time_t now, time_t *when, bool *to_on);

void pk_state_touch();  // state_rev növelése

const char *pk_weekday_short(int mon0);  // "H", "K", "Sze", ...
