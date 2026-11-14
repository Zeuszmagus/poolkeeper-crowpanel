#pragma once
// PoolKeeper ügyfélfelület adatmodellje (UI-terv 01–10).

#include <stdint.h>
#include <time.h>

#include "../app_config.h"

// ---- Csatornák ----
// Az állandó azonosító (ChannelId) független a megjelenített névtől és sorrendtől (UI-terv 09).
enum ChannelId : uint8_t {
  CH_PUMP = 0,     // Szivattyú
  CH_LIGHT,        // Világítás
  CH_NECK,         // Nyakzuhany
  CH_MASSAGE,      // Masszázs
  CH_COUNTER,      // Ellenáramoltató
  CH_AIR,          // Levegőmasszázs
  CH_SALT,         // Sóbontó
  CH_COUNT
};

// Háromállású kezelés (UI-terv 04): balról jobbra AUTO — 0 — KÉZI
enum class Mode : uint8_t { Auto = 0, Off = 1, Manual = 2 };

// Kimenet visszaigazolási állapota
enum class OutState : uint8_t { Off, On, PendingOn, PendingOff, Error };

struct Period {
  uint16_t id;      // stabil ciklusazonosító (sóbontó hivatkozik rá), 0 = üres
  uint16_t on_min;  // 0..1439
  uint16_t off_min; // 1..1439, mindig > on_min (éjfélen átnyúló nem menthető)
};

struct Program {
  uint8_t count;
  Period p[PK_MAX_PERIODS];
};

// Sóbontás a szivattyú ciklusaiban (UI-terv 10)
enum class SaltChoice : uint8_t { Skip = 0, Full = 1, Shorter = 2 };
struct SaltCycle {
  uint16_t cycle_id;   // a szivattyú Period::id-je
  SaltChoice choice;
  uint16_t on_min, off_min;  // Shorter esetén, a ciklus határain belül
  bool invalid;        // ciklus törlődött / kilóg → inaktív, "Időzítés ellenőrzése"
};
struct SaltConfig {
  uint8_t count;
  SaltCycle c[PK_MAX_PERIODS];
};

struct Channel {
  Mode mode;
  OutState out;          // a kimeneti modul által visszaigazolt állapot
  bool want_on;          // a vezérlési logika szerinti kívánt állapot
  bool program_running;  // AUTO-ban az időprogram most járatná
  bool waiting_flow;     // sóbontó: kérés van, de nincs igazolt keringés
  uint32_t cmd_sent_ms;
  Program program;       // elmentett, érvényes napi program
};

// Telepítői elrendezés (UI-terv 09) — az Óra elöl, Statisztika és Névjegy hátul rögzített.
struct Layout {
  uint8_t order[CH_COUNT];   // megjelenítési sorrend (ChannelId-k)
  bool visible[CH_COUNT];
};

enum class ClockFace : uint8_t { Analog = 0, Digital = 1 };

enum class Profile : uint8_t { Basic = 0, Full = 1 };

struct Stats {
  uint32_t today_s, yesterday_s;
  uint32_t total_s;
  int day_key;   // yyyymmdd, napváltás figyeléséhez
};

struct AppState {
  Profile profile;
  bool clock_valid;
  bool module_ok;        // kimeneti modul (Basic) / Pi (Full) elérhető
  bool flow_ok;          // igazolt keringés (áramláskapcsoló visszajelzés)
  ClockFace face;
  Channel ch[CH_COUNT];
  SaltConfig salt;
  bool salt_needs_check; // "Időzítés ellenőrzése"
  Layout layout;
  Stats stats;
  uint16_t next_period_id;
  uint32_t rev;          // minden változásnál nő
};

extern AppState g_state;

void pk_touch();
const char *pk_channel_name(ChannelId id);
const char *pk_mode_name(Mode m);       // "AUTO", "0", "KÉZI"
const char *pk_mode_desc(Mode m);       // "Időzített", "Ki", "Be"
uint32_t pk_channel_accent(ChannelId id);

// Időprogram logika (egy nap, minden nap ugyanaz; éjfélen átnyúló időszak nincs)
bool pk_program_active(const Program &pr, int now_min);
// Következő váltás percben a napon belül vagy holnap (visszaadja a perceket éjféltől, +1440 ha holnap)
bool pk_program_next_change(const Program &pr, int now_min, int *at_min, bool *to_on);
void pk_program_sort(Program &pr);

// Sóbontó időablak most aktív-e (a mentett szivattyúciklusokhoz igazítva)
bool pk_salt_window_active(int now_min);
// Szivattyúprogram változása után a sóbontó-beállítások ellenőrzése
void pk_salt_revalidate();

// Óra
bool pk_clock_now(struct tm *out);   // false, ha az óra érvénytelen
int pk_now_min();                    // -1, ha érvénytelen
void pk_set_local_time(int hour, int minute);

void fmt_hhmm_min(char *buf, int n, int minutes);
