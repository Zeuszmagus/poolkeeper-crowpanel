// Wi-Fi, NTP, beállító portál, helyi webes felület és ArduinoOTA.

#include "net.h"

#include <Arduino.h>
#include <ArduinoOTA.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <HTTPUpdateServer.h>
#include <Preferences.h>
#include <WebServer.h>
#include <WiFi.h>
#include <esp_mac.h>
#include <esp_sntp.h>
#include <time.h>

#include "../app_config.h"
#include "../model/model.h"

void ota_pull_begin();  // ota.cpp
void ota_pull_loop();

namespace {

NetStatus st = {};
Preferences np;
WebServer server(80);
HTTPUpdateServer web_updater;
DNSServer dns;
String cfg_ssid, cfg_pass;
bool ota_started = false;
bool web_started = false;
uint32_t portal_started_ms = 0, portal_connected_ms = 0;
uint32_t last_reconnect_ms = 0, disconnected_since = 0;
volatile bool sntp_synced_flag = false;

void make_identity() {
  uint8_t mac[6];
  esp_read_mac(mac, ESP_MAC_WIFI_STA);  // gyári (eFuse) MAC, a Wi-Fi indulásától függetlenül
  snprintf(st.device_id, sizeof st.device_id, "PK-%02X%02X%02X", mac[3], mac[4], mac[5]);
  snprintf(st.hostname, sizeof st.hostname, "poolkeeper-%02x%02x%02x", mac[3], mac[4], mac[5]);
  snprintf(st.ap_ssid, sizeof st.ap_ssid, "PoolKeeper-%02X%02X%02X", mac[3], mac[4], mac[5]);
  String pw = np.getString("admin", "");
  if (pw.length() < 8) {
    // Telepítői jelszó: első induláskor véletlenszerű, könnyen begépelhető karakterekből
    static const char cs[] = "abcdefghjkmnpqrstuvwxyz23456789";
    char b[9];
    for (int i = 0; i < 8; ++i) b[i] = cs[esp_random() % (sizeof cs - 1)];
    b[8] = 0;
    pw = b;
    np.putString("admin", pw);
  }
  strlcpy(st.admin_pass, pw.c_str(), sizeof st.admin_pass);
}

void sntp_cb(struct timeval *) { sntp_synced_flag = true; }

// ---------------- webes felület ----------------

const char *HTML_HEAD =
    "<!doctype html><html lang=hu><head><meta charset=utf-8>"
    "<meta name=viewport content='width=device-width,initial-scale=1'><title>PoolKeeper</title><style>"
    "body{font-family:system-ui,sans-serif;background:#051936;color:#f3f7ff;margin:0;padding:16px;max-width:520px}"
    "h1{color:#ffc928;font-size:22px}h2{font-size:17px;margin-top:22px;color:#93a9cd}"
    "a{color:#38c6ff}.c{background:#0a2852;border-radius:12px;padding:12px 14px;margin:10px 0}"
    "input,select,button{font-size:16px;padding:10px;border-radius:8px;border:1px solid #47618c;width:100%;"
    "box-sizing:border-box;margin:6px 0;background:#0b2148;color:#f3f7ff}"
    "button{background:#ffc928;color:#051936;font-weight:600;border:0}.m{color:#93a9cd;font-size:14px}"
    "td{padding:3px 8px 3px 0}</style></head><body><h1>PoolKeeper</h1>";

String esc(const String &s) {
  String o;
  for (char c : s) {
    if (c == '<') o += "&lt;";
    else if (c == '>') o += "&gt;";
    else if (c == '&') o += "&amp;";
    else if (c == '"') o += "&quot;";
    else if (c == '\'') o += "&#39;";
    else o += c;
  }
  return o;
}

bool require_admin() {
  // A portál (helyszíni, a kijelzőn indított, jelszavas Wi-Fi) alatt a Wi-Fi beállításhoz nem kell külön belépés.
  if (server.authenticate("admin", st.admin_pass)) return true;
  server.requestAuthentication(BASIC_AUTH, "PoolKeeper", "Telepítői jelszó szükséges");
  return false;
}

void page_root() {
  String h = HTML_HEAD;
  h += "<div class=c><table>";
  h += "<tr><td>Eszköz</td><td><b>" + String(st.device_id) + "</b></td></tr>";
  h += "<tr><td>Firmware</td><td>" PK_FW_VERSION " (build " + String(PK_FW_BUILD) + ")</td></tr>";
  h += "<tr><td>Wi-Fi</td><td>" + (st.connected ? esc(st.ssid) + " (" + String(st.rssi) + " dBm)" : String("nincs kapcsolat")) + "</td></tr>";
  h += "<tr><td>IP</td><td>" + String(st.ip) + "</td></tr>";
  struct tm t;
  char tb[32] = "nincs beállítva";
  if (pk_clock_now(&t)) strftime(tb, sizeof tb, "%Y-%m-%d %H:%M:%S", &t);
  h += "<tr><td>Idő</td><td>" + String(tb) + (st.time_synced ? " (internet)" : "") + "</td></tr>";
  h += "<tr><td>Frissítés</td><td>" + esc(st.ota_msg) + "</td></tr></table></div>";
  h += "<div class=c><a href=/wifi>Wi-Fi beállítás</a><br><a href=/settings>Távoli frissítés beállítása</a><br>"
       "<a href=/update>Firmware feltöltése (.bin)</a></div>";
  h += "<p class=m>Az eszköz kezelése a kijelzőn történik. Ez az oldal a telepítéshez és a karbantartáshoz való.</p>";
  h += "</body></html>";
  server.send(200, "text/html; charset=utf-8", h);
}

void page_wifi() {
  if (!st.portal && !require_admin()) return;
  String h = HTML_HEAD;
  h += "<h2>Wi-Fi hálózat kiválasztása</h2><div class=c><form method=post action=/wifi>";
  const int n = WiFi.scanNetworks(false, false);
  h += "<select name=ssid_sel onchange=\"document.getElementById('s').value=this.value\">"
       "<option value=''>— Válassz hálózatot —</option>";
  for (int i = 0; i < n && i < 25; ++i) {
    const String s = WiFi.SSID(i);
    if (!s.length()) continue;
    h += "<option value=\"" + esc(s) + "\">" + esc(s) + " (" + String(WiFi.RSSI(i)) + " dBm)</option>";
  }
  WiFi.scanDelete();
  h += "</select>";
  h += "<input id=s name=ssid placeholder='Hálózat neve (SSID)' value=\"" + esc(cfg_ssid) + "\">";
  h += "<input name=pass type=password placeholder='Wi-Fi jelszó'>";
  h += "<button>Mentés és kapcsolódás</button></form></div>";
  h += "<p class=m>Mentés után a panel csatlakozik a hálózathoz, és az internetről beállítja a pontos időt. "
       "Áramszünet után magától újracsatlakozik.</p><a href=/>Vissza</a></body></html>";
  server.send(200, "text/html; charset=utf-8", h);
}

void post_wifi() {
  if (!st.portal && !require_admin()) return;
  const String s = server.arg("ssid"), p = server.arg("pass");
  if (!s.length()) {
    server.send(400, "text/plain; charset=utf-8", "Hiányzó hálózatnév");
    return;
  }
  net_set_wifi(s.c_str(), p.c_str());
  String h = HTML_HEAD;
  h += "<div class=c>Kapcsolódás ide: <b>" + esc(s) + "</b>…<br><span id=r class=m>várakozás</span></div>"
       "<script>setInterval(()=>fetch('/status').then(r=>r.json()).then(j=>{document.getElementById('r').innerText="
       "j.connected?('Sikeres kapcsolódás, IP: '+j.ip+'. Ez a beállító hálózat hamarosan leáll.'):'kapcsolódás…'}),2000)"
       "</script></body></html>";
  server.send(200, "text/html; charset=utf-8", h);
}

void page_status_json() {
  String j = "{\"device\":\"" + String(st.device_id) + "\",\"build\":" + String(PK_FW_BUILD) +
             ",\"version\":\"" PK_FW_VERSION "\",\"connected\":" + (st.connected ? "true" : "false") + ",\"ip\":\"" +
             String(st.ip) + "\",\"time_synced\":" + (st.time_synced ? "true" : "false") + "}";
  server.send(200, "application/json", j);
}

void page_settings() {
  if (!require_admin()) return;
  if (server.method() == HTTP_POST) {
    if (server.hasArg("url")) net_set_ota_url(server.arg("url").c_str());
    if (server.hasArg("check")) ota_check_now();
  }
  String h = HTML_HEAD;
  h += "<h2>Távoli frissítés</h2><div class=c><form method=post action=/settings>"
       "<label class=m>Manifest URL (HTTPS)</label><input name=url value=\"" +
       esc(st.ota_url) + "\" placeholder='https://…/manifest.json'><button>Mentés</button></form>"
       "<form method=post action=/settings><input type=hidden name=check value=1><button>Frissítés keresése most</button>"
       "</form><p class=m>Állapot: " +
       esc(st.ota_msg) + "</p></div><p class=m>A panel óránként egyszer magától is megnézi, van-e új verzió.</p>"
       "<a href=/>Vissza</a></body></html>";
  server.send(200, "text/html; charset=utf-8", h);
}

void captive_redirect() {
  server.sendHeader("Location", String("http://") + WiFi.softAPIP().toString() + "/wifi", true);
  server.send(302, "text/plain", "");
}

void start_web() {
  if (web_started) return;
  server.on("/", HTTP_GET, page_root);
  server.on("/wifi", HTTP_GET, page_wifi);
  server.on("/wifi", HTTP_POST, post_wifi);
  server.on("/status", HTTP_GET, page_status_json);
  server.on("/settings", HTTP_ANY, page_settings);
  // Telefonok "captive portal" ellenőrző címei → beállító oldal
  for (const char *u : {"/generate_204", "/gen_204", "/hotspot-detect.html", "/connecttest.txt", "/ncsi.txt",
                        "/fwlink", "/redirect"})
    server.on(u, HTTP_ANY, []() {
      if (st.portal) captive_redirect();
      else server.send(404, "text/plain", "");
    });
  server.onNotFound([]() {
    if (st.portal) captive_redirect();
    else server.send(404, "text/plain; charset=utf-8", "Nem található");
  });
  web_updater.setup(&server, "/update", "admin", st.admin_pass);
  server.begin();
  web_started = true;
}

void start_ota_lan() {
  if (ota_started) return;
  ArduinoOTA.setHostname(st.hostname);
  ArduinoOTA.setPassword(st.admin_pass);
  ArduinoOTA.onStart([]() { Serial.println("[ota-lan] frissítés indul"); });
  ArduinoOTA.onEnd([]() { Serial.println("[ota-lan] kész, újraindulás"); });
  ArduinoOTA.onError([](ota_error_t e) { Serial.printf("[ota-lan] hiba: %d\n", (int)e); });
  ArduinoOTA.begin();  // mDNS: <hostname>.local
  ota_started = true;
}

void on_wifi_event(WiFiEvent_t ev, WiFiEventInfo_t) {
  switch (ev) {
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      st.connected = true;
      strlcpy(st.ip, WiFi.localIP().toString().c_str(), sizeof st.ip);
      strlcpy(st.ssid, WiFi.SSID().c_str(), sizeof st.ssid);
      disconnected_since = 0;
      Serial.printf("[wifi] kapcsolódva: %s, IP %s\n", st.ssid, st.ip);
      pk_touch();
      break;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
    case ARDUINO_EVENT_WIFI_STA_LOST_IP:
      if (st.connected) Serial.println("[wifi] kapcsolat megszakadt");
      st.connected = false;
      st.ip[0] = 0;
      if (!disconnected_since) disconnected_since = millis();
      pk_touch();
      break;
    default: break;
  }
}

void connect_sta() {
  if (!cfg_ssid.length()) return;
  WiFi.begin(cfg_ssid.c_str(), cfg_pass.c_str());
  last_reconnect_ms = millis();
  Serial.printf("[wifi] kapcsolódás: %s\n", cfg_ssid.c_str());
}

}  // namespace

// ---------------- publikus ----------------

const NetStatus &net_status() { return st; }

void net_begin() {
  np.begin("pknet", false);
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  make_identity();
  WiFi.setHostname(st.hostname);
  WiFi.onEvent(on_wifi_event);

  cfg_ssid = np.getString("ssid", "");
  cfg_pass = np.getString("pass", "");
  st.configured = cfg_ssid.length() > 0;
  strlcpy(st.ota_url, np.getString("otaurl", PK_OTA_DEFAULT_MANIFEST).c_str(), sizeof st.ota_url);
  snprintf(st.ota_msg, sizeof st.ota_msg, "%s", st.ota_url[0] ? "Még nem volt ellenőrzés" : "nincs beállítva");

  // Pontos idő az internetről (Europe/Vienna), óránkénti újraszinkronnal
  sntp_set_time_sync_notification_cb(sntp_cb);
  sntp_set_sync_interval(60UL * 60UL * 1000UL);
  configTzTime(PK_TZ, PK_NTP_1, PK_NTP_2, PK_NTP_3);

  Serial.printf("[net] eszköz: %s, hostname: %s.local, Wi-Fi: %s\n", st.device_id, st.hostname,
                st.configured ? cfg_ssid.c_str() : "(nincs beállítva)");
  connect_sta();
  start_web();
  ota_pull_begin();
}

void net_loop() {
  const uint32_t now = millis();
  if (sntp_synced_flag) {
    sntp_synced_flag = false;
    st.time_synced = true;
    st.last_sync_ms = now;
    g_state.clock_valid = true;
    pk_touch();
    struct tm t;
    pk_clock_now(&t);
    Serial.printf("[ntp] pontos idő: %04d-%02d-%02d %02d:%02d:%02d\n", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
                  t.tm_hour, t.tm_min, t.tm_sec);
  }
  if (st.connected) {
    st.rssi = WiFi.RSSI();
    start_ota_lan();
  } else if (st.configured && now - last_reconnect_ms > 30000) {
    // Biztonsági újrakapcsolódás (pl. a router később indul el áramszünet után)
    WiFi.disconnect(false, false);
    connect_sta();
  }
  if (ota_started) ArduinoOTA.handle();
  server.handleClient();

  if (st.portal) {
    if (st.connected && !portal_connected_ms) portal_connected_ms = now;
    // Sikeres kapcsolódás után 60 s-mal, vagy időtúllépéskor a portál leáll
    if ((portal_connected_ms && now - portal_connected_ms > 60000) || now - portal_started_ms > PK_PORTAL_TIMEOUT_MS)
      net_stop_portal();
  }
  ota_pull_loop();
}

void net_start_portal() {
  if (st.portal) return;
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(st.ap_ssid, st.admin_pass);
  dns.setErrorReplyCode(DNSReplyCode::NoError);
  dns.start(53, "*", WiFi.softAPIP());
  st.portal = true;
  portal_started_ms = millis();
  portal_connected_ms = 0;
  Serial.printf("[portal] fut: %s / %s, http://%s\n", st.ap_ssid, st.admin_pass, WiFi.softAPIP().toString().c_str());
  pk_touch();
}

void net_stop_portal() {
  if (!st.portal) return;
  dns.stop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  st.portal = false;
  Serial.println("[portal] leállítva");
  if (st.configured && !st.connected) connect_sta();
  pk_touch();
}

void net_set_wifi(const char *ssid, const char *pass) {
  cfg_ssid = ssid;
  cfg_pass = pass;
  np.putString("ssid", cfg_ssid);
  np.putString("pass", cfg_pass);
  st.configured = true;
  portal_connected_ms = 0;
  WiFi.disconnect(false, false);
  connect_sta();
  pk_touch();
}

void net_forget_wifi() {
  np.remove("ssid");
  np.remove("pass");
  cfg_ssid = "";
  cfg_pass = "";
  st.configured = false;
  WiFi.disconnect(false, true);
  pk_touch();
}

void net_set_ota_url(const char *url) {
  strlcpy(st.ota_url, url, sizeof st.ota_url);
  np.putString("otaurl", st.ota_url);
  snprintf(st.ota_msg, sizeof st.ota_msg, "%s", st.ota_url[0] ? "Cím mentve" : "nincs beállítva");
}

// Szükséges az ota.cpp-nek az állapot írásához
NetStatus &net_status_mut() { return st; }
