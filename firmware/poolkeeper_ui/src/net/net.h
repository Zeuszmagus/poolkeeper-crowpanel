#pragma once
// Hálózat: Wi-Fi (STA), NTP-idő, beállító portál (telefonról), helyi webes felület, ArduinoOTA,
// valamint óránkénti távoli frissítés-keresés (HTTPS manifest → firmware letöltés, SHA-256 ellenőrzés).

#include <stdint.h>

enum class OtaResult : uint8_t { Never, Checking, UpToDate, Downloading, Updated, NoUrl, Error };

struct NetStatus {
  char device_id[16];      // pl. PK-71D040 (MAC alapján, állandó)
  char hostname[32];       // pl. poolkeeper-71d040 (mDNS: poolkeeper-71d040.local)
  char admin_pass[12];     // telepítői jelszó: portál Wi-Fi, webes beállítás, helyi OTA
  char ap_ssid[24];        // beállító portál hálózatneve
  bool configured;         // van mentett Wi-Fi
  bool connected;
  char ssid[33];
  char ip[16];
  int rssi;
  bool portal;             // beállító portál fut
  bool time_synced;        // NTP-ből már kapott időt
  uint32_t last_sync_ms;
  // távoli frissítés
  char ota_url[192];
  OtaResult ota_result;
  char ota_msg[64];
  uint8_t ota_progress;    // 0..100
  uint32_t ota_last_check_ms;
  bool reboot_pending;
};

void net_begin();
void net_loop();
const NetStatus &net_status();

void net_start_portal();
void net_stop_portal();
void net_set_wifi(const char *ssid, const char *pass);  // mentés + kapcsolódás
void net_forget_wifi();
void net_set_ota_url(const char *url);
void ota_check_now();

// A firmware-érvényesítés (rollback) kezelése: hibátlan futás után hívandó a loop()-ból
void ota_validate_loop();
