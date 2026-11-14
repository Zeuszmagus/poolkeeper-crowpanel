// Távoli frissítés (OTA) — a panel kezdeményez, így NAT mögül is működik.
//
// Óránként egyszer (és induláskor pár perccel) lekéri a manifestet, pl.:
//   {
//     "build": 4,                        // egész szám; csak nagyobb build kerül telepítésre
//     "version": "0.4.0",
//     "url": "https://…/poolkeeper_ui-4.bin",
//     "sha256": "64 hex karakter",
//     "devices": ["PK-71D040"]           // elhagyható vagy ["*"] = minden panel
//   }
// HTTPS esetén a beépített (ESP-IDF) gyökértanúsítvány-csomaggal ellenőrzött kapcsolat.
// A letöltött kép SHA-256 összegét a telepítés előtt ellenőrizzük; eltérésnél nincs telepítés.
// Az új firmware első indulása után PK_OTA_VALIDATE_AFTER_MS hibátlan futás kell; ha előtte
// újraindul (összeomlik), a bootloader automatikusan visszaállítja az előző verziót.

#include <Arduino.h>
#include <HTTPClient.h>
#include <NetworkClientSecure.h>
#include <Update.h>
#include <esp_ota_ops.h>
#include <mbedtls/sha256.h>

#include "../app_config.h"
#include "../model/model.h"
#include "net.h"

NetStatus &net_status_mut();

extern const uint8_t x509_crt_bundle_start[] asm("_binary_x509_crt_bundle_start");
extern const uint8_t x509_crt_bundle_end[] asm("_binary_x509_crt_bundle_end");

namespace {

uint32_t next_check_ms = 0;
volatile bool check_requested = false;
TaskHandle_t task = nullptr;
bool validated = false;

void set_msg(OtaResult r, const char *fmt, ...) {
  NetStatus &st = net_status_mut();
  st.ota_result = r;
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(st.ota_msg, sizeof st.ota_msg, fmt, ap);
  va_end(ap);
  Serial.printf("[ota] %s\n", st.ota_msg);
  pk_touch();
}

// Egyszerű JSON-mezőkiolvasás (a manifest kicsi és általunk készített)
bool json_str(const String &j, const char *key, String &out) {
  const String k = String("\"") + key + "\"";
  int i = j.indexOf(k);
  if (i < 0) return false;
  i = j.indexOf(':', i + k.length());
  if (i < 0) return false;
  i = j.indexOf('"', i);
  if (i < 0) return false;
  const int e = j.indexOf('"', i + 1);
  if (e < 0) return false;
  out = j.substring(i + 1, e);
  return true;
}

bool json_int(const String &j, const char *key, long &out) {
  const String k = String("\"") + key + "\"";
  int i = j.indexOf(k);
  if (i < 0) return false;
  i = j.indexOf(':', i + k.length());
  if (i < 0) return false;
  out = j.substring(i + 1).toInt();
  return true;
}

bool targeted(const String &j, const char *device_id) {
  const int i = j.indexOf("\"devices\"");
  if (i < 0) return true;  // nincs célzás: minden panel
  const int a = j.indexOf('[', i), b = j.indexOf(']', i);
  if (a < 0 || b < 0) return true;
  const String list = j.substring(a, b);
  return list.indexOf("\"*\"") >= 0 || list.indexOf(String("\"") + device_id + "\"") >= 0;
}

void begin_client(HTTPClient &http, NetworkClient *&plain, NetworkClientSecure *&tls, const String &url) {
  // Az átirányítást magunk követjük (open_url): az ESP32 core 3.3.8 stop_ssl_socket() lenullázza a TLS-környezetet,
  // és vele a tanúsítványcsomag csatolását, így a beépített követésnél a második kapcsolat "No CA Chain" hibával bukik.
  http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
  http.setTimeout(15000);
  if (url.startsWith("https://")) {
    tls = new NetworkClientSecure();
    tls->setCACertBundle(x509_crt_bundle_start, x509_crt_bundle_end - x509_crt_bundle_start);
    http.begin(*tls, url);
  } else {
    plain = new NetworkClient();
    http.begin(*plain, url);
  }
}

void close_client(HTTPClient &http, NetworkClient *&plain, NetworkClientSecure *&tls) {
  http.end();
  delete plain;
  delete tls;
  plain = nullptr;
  tls = nullptr;
}

// GET, átirányítások követésével (legfeljebb 5), minden lépésben új TLS-klienssel.
// Visszatérés után a kapcsolat nyitva marad (a hívó zárja close_client-tel).
int open_url(HTTPClient &http, NetworkClient *&plain, NetworkClientSecure *&tls, String url) {
  static const char *keep[] = {"Location"};
  for (int hop = 0;; ++hop) {
    begin_client(http, plain, tls, url);
    http.collectHeaders(keep, 1);
    const int code = http.GET();
    const bool redirect = code == 301 || code == 302 || code == 303 || code == 307 || code == 308;
    const String loc = redirect ? http.header("Location") : String();
    if (!redirect || loc.isEmpty() || hop >= 5) {
      if (code < 0 || code >= 400) {
        char tls_err[96] = "";
        if (tls) tls->lastError(tls_err, sizeof tls_err);
        Serial.printf("[ota] HTTP %d (%s) %s\n", code, http.errorToString(code).c_str(), tls_err);
      }
      return code;
    }
    close_client(http, plain, tls);
    url = loc;
  }
}

void do_check() {
  NetStatus &st = net_status_mut();
  st.ota_last_check_ms = millis();
  if (!st.ota_url[0]) {
    set_msg(OtaResult::NoUrl, "nincs beállítva");
    return;
  }
  if (!st.connected) {
    set_msg(OtaResult::Error, "Nincs hálózat – később újra");
    return;
  }
  set_msg(OtaResult::Checking, "Frissítés keresése…");

  String manifest;
  {
    HTTPClient http;
    NetworkClient *plain = nullptr;
    NetworkClientSecure *tls = nullptr;
    const int code = open_url(http, plain, tls, String(st.ota_url) + (strchr(st.ota_url, '?') ? "&" : "?") + "id=" +
                                                    st.device_id + "&build=" + PK_FW_BUILD);
    if (code == 200) manifest = http.getString();
    close_client(http, plain, tls);
    if (code != 200) {
      set_msg(OtaResult::Error, "Manifest hiba (HTTP %d)", code);
      return;
    }
  }

  long build = 0;
  String url, sha, version;
  if (!json_int(manifest, "build", build) || !json_str(manifest, "url", url) || !json_str(manifest, "sha256", sha) ||
      sha.length() != 64) {
    set_msg(OtaResult::Error, "Hibás manifest");
    return;
  }
  json_str(manifest, "version", version);
  if (!targeted(manifest, st.device_id) || build <= PK_FW_BUILD) {
    set_msg(OtaResult::UpToDate, "Naprakész (build %d)", PK_FW_BUILD);
    return;
  }

  // Letöltés + SHA-256 ellenőrzés + írás a másik programhelyre
  set_msg(OtaResult::Downloading, "Letöltés: %s (build %ld)", version.c_str(), build);
  HTTPClient http;
  NetworkClient *plain = nullptr;
  NetworkClientSecure *tls = nullptr;
  const int code = open_url(http, plain, tls, url);
  bool ok = false;
  if (code == 200) {
    const int total = http.getSize();
    if (Update.begin(total > 0 ? total : UPDATE_SIZE_UNKNOWN, U_FLASH)) {
      mbedtls_sha256_context ctx;
      mbedtls_sha256_init(&ctx);
      mbedtls_sha256_starts(&ctx, 0);
      NetworkClient *s = http.getStreamPtr();
      static uint8_t buf[4096];
      int done = 0;
      uint32_t last_data = millis();
      while (http.connected() && (total <= 0 || done < total)) {
        const int avail = s->available();
        if (avail <= 0) {
          if (millis() - last_data > 20000) break;
          delay(5);
          continue;
        }
        const int n = s->readBytes(buf, avail > (int)sizeof buf ? sizeof buf : avail);
        if (n <= 0) continue;
        last_data = millis();
        mbedtls_sha256_update(&ctx, buf, n);
        if (Update.write(buf, n) != (size_t)n) break;
        done += n;
        if (total > 0) st.ota_progress = (uint8_t)(100LL * done / total);
      }
      uint8_t digest[32];
      mbedtls_sha256_finish(&ctx, digest);
      mbedtls_sha256_free(&ctx);
      char hex[65];
      for (int i = 0; i < 32; ++i) sprintf(hex + 2 * i, "%02x", digest[i]);
      sha.toLowerCase();
      if ((total > 0 && done != total) || sha != hex) {
        Update.abort();
        set_msg(OtaResult::Error, total > 0 && done != total ? "Megszakadt letöltés" : "SHA-256 eltérés – nincs telepítés");
      } else if (Update.end(true)) {
        ok = true;
      } else {
        set_msg(OtaResult::Error, "Telepítési hiba (%d)", Update.getError());
      }
    } else {
      set_msg(OtaResult::Error, "Nincs hely a frissítésnek");
    }
  } else {
    set_msg(OtaResult::Error, "Letöltési hiba (HTTP %d)", code);
  }
  close_client(http, plain, tls);
  if (ok) {
    set_msg(OtaResult::Updated, "Telepítve: %s – újraindulás", version.c_str());
    st.reboot_pending = true;
  }
}

void ota_task(void *) {
  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    do_check();
  }
}

}  // namespace

void ota_pull_begin() {
  next_check_ms = millis() + PK_OTA_FIRST_CHECK_MS + (esp_random() % 60000);
  // TLS + HTTP: nagyobb verem; a letöltés nem akasztja meg a kijelzőt
  xTaskCreatePinnedToCore(ota_task, "ota", 12288, nullptr, 1, &task, 0);
}

void ota_pull_loop() {
  const uint32_t now = millis();
  const bool due = (int32_t)(now - next_check_ms) >= 0;
  if ((due || check_requested) && task) {
    check_requested = false;
    // Óránként egyszer + kis véletlen eltolás (sok panel ne egyszerre kérdezzen)
    next_check_ms = now + PK_OTA_CHECK_PERIOD_MS + (esp_random() % 120000);
    xTaskNotifyGive(task);
  }
  if (net_status().reboot_pending) {
    static uint32_t t0 = 0;
    if (!t0) t0 = now;
    if (now - t0 > 3000) {
      Serial.println("[ota] újraindítás az új firmware-rel");
      Serial.flush();
      ESP.restart();
    }
  }
}

void ota_check_now() { check_requested = true; }

// Új firmware érvényesítése: hibátlan futás után véglegesítjük, különben a bootloader visszaáll.
void ota_validate_loop() {
  if (validated || millis() < PK_OTA_VALIDATE_AFTER_MS) return;
  validated = true;
  const esp_partition_t *running = esp_ota_get_running_partition();
  esp_ota_img_states_t state;
  if (esp_ota_get_state_partition(running, &state) == ESP_OK && state == ESP_OTA_IMG_PENDING_VERIFY) {
    esp_ota_mark_app_valid_cancel_rollback();
    Serial.println("[ota] új firmware érvényesítve");
  }
}

// Az Arduino core ne érvényesítse automatikusan az új képet induláskor — mi tesszük meg hibátlan futás után.
extern "C" bool verifyRollbackLater() { return true; }
