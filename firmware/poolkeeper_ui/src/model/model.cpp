#include "model.h"

#include <stdio.h>
#include <string.h>
#include <sys/time.h>

AppState g_state = {};

void pk_touch() { g_state.rev++; }

const char *pk_channel_name(ChannelId id) {
  static const char *names[CH_COUNT] = {"Szivattyú",       "Világítás",      "Nyakzuhany", "Masszázs",
                                        "Ellenáramoltató", "Levegőmasszázs", "Sóbontó"};
  return id < CH_COUNT ? names[id] : "?";
}

const char *pk_mode_name(Mode m) {
  switch (m) {
    case Mode::Auto: return "AUTO";
    case Mode::Off: return "0";
    case Mode::Manual: return "KÉZI";
  }
  return "?";
}

const char *pk_mode_desc(Mode m) {
  switch (m) {
    case Mode::Auto: return "Időzített";
    case Mode::Off: return "Ki";
    case Mode::Manual: return "Be";
  }
  return "";
}

uint32_t pk_channel_accent(ChannelId id) {
  // Jól elkülönülő árnyalatok (a sárga a kijelölésé, a piros a hibáé):
  // kék, narancs, türkiz, ibolya, zöld, rózsaszín, ezüst
  static const uint32_t c[CH_COUNT] = {0x3D8BFF, 0xFF8A3D, 0x2EE6D6, 0xB57BFF, 0x7BE35A, 0xFF5FA2, 0xE8EEF7};
  return id < CH_COUNT ? c[id] : 0xFFFFFF;
}

bool pk_program_active(const Program &pr, int now_min) {
  for (int i = 0; i < pr.count; ++i)
    if (now_min >= pr.p[i].on_min && now_min < pr.p[i].off_min) return true;
  return false;
}

bool pk_program_next_change(const Program &pr, int now_min, int *at_min, bool *to_on) {
  if (pr.count == 0) return false;
  const bool cur = pk_program_active(pr, now_min);
  for (int m = now_min + 1; m < now_min + 1 + 1440; ++m) {
    if (pk_program_active(pr, m % 1440) != cur) {
      *at_min = m;
      *to_on = !cur;
      return true;
    }
  }
  return false;
}

void pk_program_sort(Program &pr) {
  for (int i = 1; i < pr.count; ++i)
    for (int j = i; j > 0 && pr.p[j].on_min < pr.p[j - 1].on_min; --j) {
      Period t = pr.p[j];
      pr.p[j] = pr.p[j - 1];
      pr.p[j - 1] = t;
    }
}

static const Period *find_pump_period(uint16_t id) {
  const Program &pr = g_state.ch[CH_PUMP].program;
  for (int i = 0; i < pr.count; ++i)
    if (pr.p[i].id == id) return &pr.p[i];
  return nullptr;
}

bool pk_salt_window_active(int now_min) {
  const SaltConfig &sc = g_state.salt;
  for (int i = 0; i < sc.count; ++i) {
    const SaltCycle &c = sc.c[i];
    if (c.invalid || c.choice == SaltChoice::Skip) continue;
    const Period *p = find_pump_period(c.cycle_id);
    if (!p) continue;
    // Az időablak nem tolódik: mindig a ciklus (vagy a rövidebb szakasz) óraidejéhez kötött.
    const int a = c.choice == SaltChoice::Full ? p->on_min : c.on_min;
    const int b = c.choice == SaltChoice::Full ? p->off_min : c.off_min;
    if (now_min >= a && now_min < b) return true;
  }
  return false;
}

// A szivattyúprogram mentése után: a sóbontó-lista a szivattyúciklusokat követi (stabil azonosító szerint).
//  - új ciklus: alapból Kihagyás
//  - megszűnt ciklus, amelyhez sóbontás tartozott: a beállítás megszűnik, "Időzítés ellenőrzése" jelzés
//  - rövidebb szakasz, amely kilóg a módosított ciklusból: inaktív (invalid), jelzés
void pk_salt_revalidate() {
  const SaltConfig old = g_state.salt;
  const Program &pr = g_state.ch[CH_PUMP].program;
  SaltConfig nc = {};
  bool flag = g_state.salt_needs_check;
  for (int i = 0; i < old.count; ++i)
    if (!find_pump_period(old.c[i].cycle_id) && old.c[i].choice != SaltChoice::Skip) flag = true;
  for (int k = 0; k < pr.count; ++k) {
    const Period &p = pr.p[k];
    SaltCycle c = {p.id, SaltChoice::Skip, p.on_min, p.off_min, false};
    for (int i = 0; i < old.count; ++i)
      if (old.c[i].cycle_id == p.id) c = old.c[i];
    if (c.choice == SaltChoice::Shorter && (c.on_min < p.on_min || c.off_min > p.off_min || c.off_min <= c.on_min))
      c.invalid = true;
    if (c.invalid) flag = true;
    nc.c[nc.count++] = c;
  }
  g_state.salt = nc;
  g_state.salt_needs_check = flag;
}

bool pk_clock_now(struct tm *out) {
  time_t now = time(nullptr);
  if (!g_state.clock_valid) return false;
  localtime_r(&now, out);
  return true;
}

int pk_now_min() {
  struct tm t;
  if (!pk_clock_now(&t)) return -1;
  return t.tm_hour * 60 + t.tm_min;
}

void pk_set_local_time(int hour, int minute) {
  time_t now = time(nullptr);
  struct tm t;
  localtime_r(&now, &t);
  if (!g_state.clock_valid || t.tm_year < 124) {
    // Dátum még nincs beállítva (nyitott döntés): a firmware fordítási dátuma
    static const char *months = "JanFebMarAprMayJunJulAugSepOctNovDec";
    char mon[4] = {0};
    int day = 1, year = 2026;
    sscanf(__DATE__, "%3s %d %d", mon, &day, &year);
    memset(&t, 0, sizeof t);
    t.tm_mon = (int)((strstr(months, mon) - months) / 3);
    t.tm_mday = day;
    t.tm_year = year - 1900;
  }
  t.tm_hour = hour;
  t.tm_min = minute;
  t.tm_sec = 0;
  t.tm_isdst = -1;
  struct timeval tv = {mktime(&t), 0};
  settimeofday(&tv, nullptr);
  g_state.clock_valid = true;
  pk_touch();
}

void fmt_hhmm_min(char *buf, int n, int minutes) {
  minutes = ((minutes % 1440) + 1440) % 1440;
  snprintf(buf, n, "%02d:%02d", minutes / 60, minutes % 60);
}
