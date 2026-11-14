#pragma once
// PoolKeeper CrowPanel ügyfélfelület — fordítási idejű beállítások.

#include <stdint.h>

#define PK_FW_VERSION "0.4.2"
// Egész szám, minden kiadásnál növelni kell: a távoli frissítés ezzel hasonlít (manifest "build").
#define PK_FW_BUILD 7

// Időzóna (POSIX TZ): Europe/Vienna
#define PK_TZ "CET-1CEST,M3.5.0,M10.5.0/3"

// Névjegy — a végleges elérhetőséget Zsolti adja meg (UI-terv 08).
#define PK_SUPPORT_PHONE ""
#define PK_SUPPORT_EMAIL ""

// Kijelző
static constexpr int PK_SCREEN_W = 360;
static constexpr int PK_SCREEN_H = 360;
static constexpr uint8_t PK_BACKLIGHT_PCT = 75;

// Tétlenség után vissza az órához (UI-terv 01, nyitott döntés — javasolt érték)
static constexpr uint32_t PK_IDLE_TO_CLOCK_MS = 60000;
// Szerkesztő nézetekben hosszabb türelmi idő; mentetlen módosítás elvész
static constexpr uint32_t PK_IDLE_EDITOR_MS = 180000;

// Kimeneti modul visszaigazolás időkorlátja
static constexpr uint32_t PK_CMD_TIMEOUT_MS = 5000;

// Napi időprogram: legfeljebb ennyi BE–KI időszak csatornánként (UI-terv 05)
static constexpr int PK_MAX_PERIODS = 5;

// ---- Áramszünet utáni visszaállás ----
// Az AUTO üzemmód mindig visszaáll (az időprogram fut tovább, amint érvényes az idő).
// KÉZI üzemmód: biztonsági okból 0-ra áll vissza, hogy semmi ne induljon el magától kézi bekapcsolással.
static constexpr bool PK_RESTORE_MANUAL = false;

// ---- Hálózat ----
#define PK_NTP_1 "pool.ntp.org"
#define PK_NTP_2 "time.google.com"
#define PK_NTP_3 "time.cloudflare.com"
// Beállító portál (telefonról) ennyi idő után magától leáll
static constexpr uint32_t PK_PORTAL_TIMEOUT_MS = 15UL * 60UL * 1000UL;

// ---- Távoli frissítés (OTA) ----
// A panel óránként egyszer lekéri a manifestet (HTTPS). Az URL a webes beállításoknál vagy a
// soros konzolon (otaurl) adható meg; üresen hagyva nincs automatikus frissítés-keresés.
#define PK_OTA_DEFAULT_MANIFEST "https://github.com/Zeuszmagus/poolkeeper-crowpanel/releases/latest/download/manifest.json"
static constexpr uint32_t PK_OTA_CHECK_PERIOD_MS = 60UL * 60UL * 1000UL;
static constexpr uint32_t PK_OTA_FIRST_CHECK_MS = 3UL * 60UL * 1000UL;   // indulás után
// Új firmware indulása után ennyi hibátlan futás kell az érvényesítéshez; előtte újraindulás = visszaállás
static constexpr uint32_t PK_OTA_VALIDATE_AFTER_MS = 60UL * 1000UL;
