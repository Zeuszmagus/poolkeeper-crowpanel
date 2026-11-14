// Soros diagnosztikai konzol (115200, USB-Serial/JTAG).
// Parancsok egy sorban, Enterrel lezárva — lásd "help".

#include "console.h"

#include <Arduino.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#include "../backend/backend.h"
#include "../hal/board.h"
#include "../model/model.h"
#include "../ui/ui.h"

namespace {

char line[96];
size_t len = 0;

void help() {
  Serial.println(
      "PoolKeeper konzol:\n"
      "  r [n] / l [n]      gyűrű jobbra / balra (n lépés)\n"
      "  c | d | L          nyomás | dupla nyomás | hosszú nyomás\n"
      "  tap X Y            koppintás\n"
      "  swipe X0 Y0 X1 Y1  húzás\n"
      "  page N             ugrás az N. oldalra (0 = Medence)\n"
      "  link 0|1           kapcsolat szimuláció\n"
      "  fault 0|1          ORP érzékelőhiba szimuláció\n"
      "  clock 0|1          óra érvénytelen / érvényes\n"
      "  time YYYY-MM-DD HH:MM   helyi idő beállítása (Europe/Vienna)\n"
      "  sched I DAYSHEX HH:MM HH:MM  időablak (bit0=hétfő), pl. sched 1 08 21:00 00:30\n"
      "  profile basic|full\n"
      "  status | shot | help");
}

void status() {
  const AppState &s = g_state;
  time_t now = time(nullptr);
  char tb[32];
  struct tm lt;
  localtime_r(&now, &lt);
  strftime(tb, sizeof tb, "%Y-%m-%d %H:%M:%S %Z", &lt);
  Serial.printf("profil=%s link=%d clock=%d(%s) oldal=%s heap=%u psram=%u\n", s.profile == Profile::Full ? "full" : "basic",
                s.link_ok, s.clock_valid, tb, ui_page_name(), (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getFreePsram());
  for (int i = 0; i < s.output_count; ++i) {
    const Output &o = s.outputs[i];
    const Schedule &sc = s.schedules[i];
    Serial.printf("  %-8s on=%d mode=%s cmd=%d id=%lu sched=%02X %02u:%02u-%02u:%02u", o.key, o.on,
                  o.mode == OutputMode::Manual ? "kezi" : "auto", (int)o.cmd, (unsigned long)o.cmd_id, sc.days,
                  sc.start_min / 60, sc.start_min % 60, sc.end_min / 60, sc.end_min % 60);
    time_t when;
    bool to_on;
    if (o.has_schedule && pk_schedule_next_change(sc, now, &when, &to_on)) {
      localtime_r(&when, &lt);
      strftime(tb, sizeof tb, "%a %H:%M %Z", &lt);
      Serial.printf(" next=%s@%s", to_on ? "BE" : "KI", tb);
    }
    Serial.println();
  }
}

bool set_local_time(const char *arg) {
  int Y, M, D, h, m;
  if (sscanf(arg, "%d-%d-%d %d:%d", &Y, &M, &D, &h, &m) != 5) return false;
  struct tm t = {};
  t.tm_year = Y - 1900;
  t.tm_mon = M - 1;
  t.tm_mday = D;
  t.tm_hour = h;
  t.tm_min = m;
  t.tm_isdst = -1;
  time_t e = mktime(&t);
  struct timeval tv = {e, 0};
  settimeofday(&tv, nullptr);
  return true;
}

void exec(char *cmd) {
  char *arg = strchr(cmd, ' ');
  if (arg) *arg++ = 0;
  const int n = arg ? atoi(arg) : 1;
  auto knob = [](KnobEvent e, int k) {
    for (int i = 0; i < (k < 1 ? 1 : k); ++i) board_inject_knob(e);
  };
  if (!strcmp(cmd, "r")) knob(KnobEvent::Cw, n);
  else if (!strcmp(cmd, "l")) knob(KnobEvent::Ccw, n);
  else if (!strcmp(cmd, "c")) knob(KnobEvent::Click, 1);
  else if (!strcmp(cmd, "d")) knob(KnobEvent::DoubleClick, 1);
  else if (!strcmp(cmd, "L")) knob(KnobEvent::LongPress, 1);
  else if (!strcmp(cmd, "tap") && arg) {
    int x, y;
    if (sscanf(arg, "%d %d", &x, &y) == 2) board_inject_touch(x, y, x, y, 80);
  } else if (!strcmp(cmd, "swipe") && arg) {
    int a, b, c, d;
    if (sscanf(arg, "%d %d %d %d", &a, &b, &c, &d) == 4) board_inject_touch(a, b, c, d, 300);
  } else if (!strcmp(cmd, "page")) ui_goto_page(arg ? atoi(arg) : 0);
  else if (!strcmp(cmd, "link")) backend().sim_set_link(n != 0);
  else if (!strcmp(cmd, "fault")) backend().sim_set_sensor_fault(n != 0);
  else if (!strcmp(cmd, "clock")) backend().sim_set_clock_valid(n != 0);
  else if (!strcmp(cmd, "time") && arg) {
    if (set_local_time(arg)) {
      g_state.clock_valid = true;
      g_state.clock_simulated = true;
      pk_state_touch();
    } else {
      Serial.println("ERR formátum: time 2026-03-29 01:59");
    }
  } else if (!strcmp(cmd, "sched") && arg) {
    unsigned idx, days, h0, m0, h1, m1;
    if (sscanf(arg, "%u %x %u:%u %u:%u", &idx, &days, &h0, &m0, &h1, &m1) != 6) {
      Serial.println("ERR formátum: sched 0 7F 08:00 20:00");
      return;
    }
    Schedule sc = {true, (uint8_t)(days & 0x7F), (uint16_t)(h0 * 60 + m0), (uint16_t)(h1 * 60 + m1)};
    if (!backend().save_schedule((int)idx, sc)) {
      Serial.println("ERR elutasítva");
      return;
    }
  } else if (!strcmp(cmd, "profile") && arg) {
    backend_switch_profile(!strcmp(arg, "full") ? Profile::Full : Profile::Basic);
    ui_request_rebuild();
  } else if (!strcmp(cmd, "status")) status();
  else if (!strcmp(cmd, "shot")) ui_send_screenshot();
  else if (!strcmp(cmd, "help")) help();
  else {
    Serial.printf("ERR ismeretlen parancs: %s\n", cmd);
    return;
  }
  Serial.printf("OK %s\n", cmd);
}

}  // namespace

void console_poll() {
  while (Serial.available()) {
    const int c = Serial.read();
    if (c == '\r') continue;
    if (c == '\n') {
      line[len] = 0;
      if (len) exec(line);
      len = 0;
    } else if (len < sizeof(line) - 1) {
      line[len++] = (char)c;
    }
  }
}
