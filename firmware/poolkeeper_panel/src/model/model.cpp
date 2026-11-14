#include "model.h"

AppState g_state = {};

void pk_state_touch() { g_state.state_rev++; }

int pk_weekday_mon0(const struct tm &t) { return (t.tm_wday + 6) % 7; }

static bool day_set(const Schedule &s, int mon0) { return (s.days >> mon0) & 1; }

bool pk_schedule_active(const Schedule &s, const struct tm &local) {
  if (!s.enabled || s.days == 0 || s.start_min == s.end_min) return false;
  const int now_min = local.tm_hour * 60 + local.tm_min;
  const int today = pk_weekday_mon0(local);
  const int yesterday = (today + 6) % 7;
  if (s.start_min < s.end_min) {
    return day_set(s, today) && now_min >= s.start_min && now_min < s.end_min;
  }
  // Éjfélen átnyúló ablak: [start, 24:00) a kijelölt napon, [0, end) a következő napon.
  if (day_set(s, today) && now_min >= s.start_min) return true;
  if (day_set(s, yesterday) && now_min < s.end_min) return true;
  return false;
}

bool pk_schedule_next_change(const Schedule &s, time_t now, time_t *when, bool *to_on) {
  struct tm t;
  localtime_r(&now, &t);
  const bool cur = pk_schedule_active(s, t);
  // Percre kerekítés felfelé, majd léptetés.
  time_t probe = now - (now % 60) + 60;
  for (int i = 0; i < 8 * 1440; ++i, probe += 60) {
    localtime_r(&probe, &t);
    if (pk_schedule_active(s, t) != cur) {
      *when = probe;
      *to_on = !cur;
      return true;
    }
  }
  return false;
}

const char *pk_weekday_short(int mon0) {
  static const char *names[7] = {"H", "K", "Sze", "Cs", "P", "Szo", "V"};
  return (mon0 >= 0 && mon0 < 7) ? names[mon0] : "?";
}
