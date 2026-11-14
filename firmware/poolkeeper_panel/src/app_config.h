#pragma once
// PoolKeeper CrowPanel — fordítási idejű beállítások.

#include <stdint.h>

#define PK_FW_VERSION "0.1.0-m1"

// Időzóna (POSIX TZ): Europe/Vienna, CET/CEST nyári időszámítással.
#define PK_TZ "CET-1CEST,M3.5.0,M10.5.0/3"

// Parancs lejárati ideje: ennyi idő alatt kell visszaigazolást kapni.
static constexpr uint32_t PK_CMD_TIMEOUT_MS = 8000;

// Mérési adat ennyi idő után számít elavultnak.
// Demóban rövid, hogy a kapcsolatvesztés hatása gyorsan látható legyen.
static constexpr uint32_t PK_STALE_AFTER_MS = 30000;

// Kézi felülbírálás alapértelmezett időtartama (perc).
static constexpr uint16_t PK_OVERRIDE_DEFAULT_MIN = 120;

// Kijelző
static constexpr int PK_SCREEN_W = 360;
static constexpr int PK_SCREEN_H = 360;
static constexpr uint8_t PK_BACKLIGHT_PCT = 70;
