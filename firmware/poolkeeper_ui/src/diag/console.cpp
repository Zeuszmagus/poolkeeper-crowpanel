#include "console.h"

#include <Arduino.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#include "../backend/controller.h"
#include "../hal/board.h"
#include "../model/model.h"
#include "../model/storage.h"
#include "../net/net.h"
#include <esp_ota_ops.h>
#include "../ui/ui.h"

namespace {
char line[256];
size_t len = 0;

void help() {
  Serial.println(
      "PoolKeeper UI konzol:\n"
      "  r [n] / l [n]        gyűrű jobbra / balra\n"
      "  c | d | L            nyomás | dupla | hosszú nyomás\n"
      "  tap X Y              koppintás\n"
      "  swipe X0 Y0 X1 Y1    húzás (vízszintes: lapozás, függőleges: érték)\n"
      "  time YYYY-MM-DD HH:MM  helyi idő | clock 0  óra érvénytelenítése\n"
      "  module 0|1           kimeneti modul elérhető\n"
      "  flow 0|1             keringés-visszajelzés hiba (0 = hiba)\n"
      "  prog CH HH:MM-HH:MM [...]  program közvetlen mentése (CH 0..6)\n"
      "  mode CH auto|off|man\n"
      "  wifi SSID [JELSZO] | wififorget | portal 1|0 | otaurl URL | otacheck | net\n"
      "  profile basic|full | nvsreset | status | shot | help");
}

void status() {
  const AppState &s = g_state;
  time_t now = time(nullptr);
  struct tm lt;
  localtime_r(&now, &lt);
  char tb[32];
  strftime(tb, sizeof tb, "%Y-%m-%d %H:%M:%S %Z", &lt);
  Serial.printf("nezet=%s ora=%s(%d) modul=%d aramlas=%d heap=%u psram=%u\n", ui_view_name(), tb, s.clock_valid,
                s.module_ok, s.flow_ok, (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getFreePsram());
  for (int i = 0; i < CH_COUNT; ++i) {
    const Channel &c = s.ch[i];
    Serial.printf("  %d %-16s mode=%-4s out=%d want=%d run=%d wait=%d prog=", i, pk_channel_name((ChannelId)i),
                  pk_mode_name(c.mode), (int)c.out, c.want_on, c.program_running, c.waiting_flow);
    for (int k = 0; k < c.program.count; ++k)
      Serial.printf("[%u]%02u:%02u-%02u:%02u ", c.program.p[k].id, c.program.p[k].on_min / 60, c.program.p[k].on_min % 60,
                    c.program.p[k].off_min / 60, c.program.p[k].off_min % 60);
    Serial.println();
  }
  Serial.printf("  salt: check=%d", s.salt_needs_check);
  for (int i = 0; i < s.salt.count; ++i)
    Serial.printf(" [%u:%d%s %02u:%02u-%02u:%02u]", s.salt.c[i].cycle_id, (int)s.salt.c[i].choice, s.salt.c[i].invalid ? "!" : "",
                  s.salt.c[i].on_min / 60, s.salt.c[i].on_min % 60, s.salt.c[i].off_min / 60, s.salt.c[i].off_min % 60);
  Serial.printf("\n  stats: ma=%lus tegnap=%lus ossz=%lus\n", (unsigned long)s.stats.today_s,
                (unsigned long)s.stats.yesterday_s, (unsigned long)s.stats.total_s);
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
    if (sscanf(arg, "%d %d %d %d", &a, &b, &c, &d) == 4) board_inject_touch(a, b, c, d, 350);
  } else if (!strcmp(cmd, "time") && arg) {
    int Y, M, D, h, m;
    if (sscanf(arg, "%d-%d-%d %d:%d", &Y, &M, &D, &h, &m) != 5) {
      Serial.println("ERR formátum: time 2026-11-12 14:30");
      return;
    }
    struct tm t = {};
    t.tm_year = Y - 1900;
    t.tm_mon = M - 1;
    t.tm_mday = D;
    t.tm_hour = h;
    t.tm_min = m;
    t.tm_isdst = -1;
    struct timeval tv = {mktime(&t), 0};
    settimeofday(&tv, nullptr);
    g_state.clock_valid = true;
    pk_touch();
  } else if (!strcmp(cmd, "clock")) {
    g_state.clock_valid = n != 0;
    pk_touch();
  } else if (!strcmp(cmd, "module")) controller_sim_module(n != 0);
  else if (!strcmp(cmd, "flow")) controller_sim_flow_fault(n == 0);
  else if (!strcmp(cmd, "mode") && arg) {
    char m[8] = {0};
    int ch;
    if (sscanf(arg, "%d %7s", &ch, m) == 2 && ch >= 0 && ch < CH_COUNT)
      controller_set_mode((ChannelId)ch, !strcmp(m, "auto") ? Mode::Auto : !strcmp(m, "man") ? Mode::Manual : Mode::Off);
  } else if (!strcmp(cmd, "prog") && arg) {
    int ch = atoi(arg);
    Program p = {};
    const char *s = strchr(arg, ' ');
    while (s && p.count < PK_MAX_PERIODS) {
      int h0, m0, h1, m1;
      if (sscanf(s, " %d:%d-%d:%d", &h0, &m0, &h1, &m1) != 4) break;
      // meglévő azonosító megtartása azonos sorszámnál (stabil ciklus-id teszteléséhez)
      const Program &old = g_state.ch[ch].program;
      const uint16_t id = p.count < old.count ? old.p[p.count].id : 0;
      p.p[p.count++] = {id, (uint16_t)(h0 * 60 + m0), (uint16_t)(h1 * 60 + m1)};
      s = strchr(s + 1, ' ');
    }
    if (ch < 0 || ch >= CH_COUNT || !controller_save_program((ChannelId)ch, p)) {
      Serial.println("ERR érvénytelen program");
      return;
    }
  } else if (!strcmp(cmd, "profile") && arg) {
    g_state.profile = !strcmp(arg, "full") ? Profile::Full : Profile::Basic;
    pk_touch();
  } else if (!strcmp(cmd, "nvsreset")) {
    storage_factory_reset();
    Serial.println("NVS törölve — újraindítás");
    delay(100);
    ESP.restart();
  } else if (!strcmp(cmd, "wifi") && arg) {
    // Formátum: "wifi NÉV|JELSZÓ" (a név tartalmazhat szóközt), vagy "wifi NÉV JELSZÓ"
    char ssid[33] = {0}, pass[65] = {0};
    const char *bar = strchr(arg, '|');
    if (bar) {
      strlcpy(ssid, arg, (size_t)(bar - arg + 1) < sizeof ssid ? (size_t)(bar - arg + 1) : sizeof ssid);
      strlcpy(pass, bar + 1, sizeof pass);
    } else {
      sscanf(arg, "%32s %64s", ssid, pass);
    }
    net_set_wifi(ssid, pass);
    Serial.printf("wifi: \"%s\" mentve\n", ssid);
    return;  // a jelszót nem írjuk vissza a konzolra
  } else if (!strcmp(cmd, "wififorget")) net_forget_wifi();
  else if (!strcmp(cmd, "portal")) {
    if (n) net_start_portal();
    else net_stop_portal();
  } else if (!strcmp(cmd, "otaurl")) net_set_ota_url(arg ? arg : "");
  else if (!strcmp(cmd, "otacheck")) ota_check_now();
  else if (!strcmp(cmd, "net")) {
    const NetStatus &s = net_status();
    Serial.printf("eszkoz=%s host=%s.local wifi=%s conf=%d conn=%d ip=%s rssi=%d portal=%d ntp=%d\n", s.device_id,
                  s.hostname, s.ssid, s.configured, s.connected, s.ip, s.rssi, s.portal, s.time_synced);
    Serial.printf("ota_url=%s\nota=%s (%d%%) build=%d admin=%s\n", s.ota_url, s.ota_msg, s.ota_progress, PK_FW_BUILD,
                  s.admin_pass);
    const esp_partition_t *run = esp_ota_get_running_partition();
    esp_ota_img_states_t ost = ESP_OTA_IMG_UNDEFINED;
    esp_ota_get_state_partition(run, &ost);
    Serial.printf("futó partíció=%s állapot=%d (0=new 1=pending 2=valid 3=invalid 4=aborted -1=undefined)\n",
                  run ? run->label : "?", (int)ost);
  } else if (!strcmp(cmd, "reboot")) {
    Serial.println("újraindítás");
    Serial.flush();
    ESP.restart();
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
