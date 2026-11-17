# PoolKeeper CrowPanel — kijelző-firmware

Magyar nyelvű LVGL-felület az Elecrow CrowPanel 1,46″ kerek kijelzőre.
Tervdokumentumok: [PoolKeeper_CrowPanel_projektterv.md](PoolKeeper_CrowPanel_projektterv.md), [PoolKeeper_CrowPanel_UI_terv.md](PoolKeeper_CrowPanel_UI_terv.md).

| Sketch | Tartalom |
|---|---|
| **`firmware/poolkeeper_ui/`** (alapértelmezett) | Ügyfélfelület az UI-terv szerint: óra (analóg/digitális), egyelemes körmenü, időbeállítás, 7 vezérlőcsatorna AUTO / 0 / KÉZI kezeléssel, napi időprogram (max. 5 időszak), sóbontó ciklusválasztással, statisztika, névjegy, telepítői beállítások. |
| `firmware/poolkeeper_panel/` | Korábbi M1 demó (Basic/Full szimuláció, vízértékek). Fordítás: `$env:PK_SKETCH='poolkeeper_panel'`, majd a szokásos szkriptek. |

## Ügyfélfelület (poolkeeper_ui) — kezelés

- **Óra:** a kijelző érintése vagy a gomb rövid megnyomása megnyitja a **főképernyőt**. Érvénytelen órán az „Állítsd be az órát” felirat látszik.
- **Főképernyő** (2026-11-14): idő és nap; kiemelve a Szivattyú (BE/KI, üzemmód, következő kapcsolás); a többi eszköz kis ikonnal (saját szín = BE, szürke = KI, alatta A / 0 / K); állapotsor („Minden rendben” vagy a legfontosabb figyelmeztetés); a peremen 24 órás gyűrű a szivattyú programjával. Nyomás vagy koppintás: menü. Hosszú nyomás: vissza az órához. A menüből hosszú nyomással a főképernyőre lehet visszalépni.
- **Menüsorrend** (az ügyfél kérésére 2026-11-13-tól, eltérve az UI-terv 06. pontjától): a csatornák a telepítői sorrend szerint (első a Szivattyú), utánuk Statisztika, Névjegy, a végén az Óra. A menü megnyitáskor mindig az első ponton áll.
- **Menü:** forgatással vagy vízszintes húzással lapozol, a két oldali nyíl mutatja az irányt. Nyomás vagy középre koppintás: megnyitás. Dupla vagy hosszú nyomás: vissza az órához. 60 s tétlenség után a panel magától visszatér az órához (szerkesztőkben 180 s után).
- **Óra pont:** nyomásra a pontos idő beállítása nyílik (óra → nyomás → perc → nyomás = érvényesítés). A számlap analóg/digitális váltása hosszú nyomással vagy a „Számlap” sorra koppintva történik. Ezt a megoldást még jóvá kell hagyni.
- **Csatornák** (Szivattyú, Világítás, Nyakzuhany, Masszázs, Ellenáramoltató, Levegőmasszázs, Sóbontó): a jobbra forgatás AUTO → 0 → KÉZI irányba lép, a balra forgatás visszafelé. AUTO módban a nyomás megnyitja az időprogramot. A kijelző a kimenet állapotát a modul visszaigazolása alapján mutatja.
- **Időprogram:** 24 órás körszámlap. Forgatással választhatsz időszakot, az „Új időszak” pontot vagy a Mentést. Szerkesztés: BE óra → BE perc → KI óra → KI perc. A forgatás vagy a függőleges húzás állítja az értéket, a nyomás a következő mezőre lép. Hibaüzenetek: „Add meg a be- és kikapcsolás idejét”, „A kikapcsolás ideje legyen későbbi”.
- **Sóbontó:** ciklusonként Kihagyás, Teljes ciklus vagy Rövidebb időszak, a ciklus határain belül. Igazolt keringés nélkül „Keringésre vár” jelenik meg. Ha a szivattyúciklus megváltozik, az érintett beállítás inaktív lesz, és megjelenik az „Időzítés ellenőrzése” jelzés.
- **Telepítői beállítások:** a Névjegy oldalon hosszú nyomással nyílnak meg. Itt állítható a megjelenítés, és itt lehet átrendezni a menüt; a Mentés NVS-be ír.
- **Tárolás (NVS):** a számlap, a programok, a sóbontó és az elrendezés megmarad, az összes üzemidő 10 percenként mentődik. Az üzemmódok **nem** maradnak meg: újraindítás után minden csatorna 0 (Ki) állásban indul.

Konzolparancsok (`help`): `r/l/c/d/L`, `tap`, `swipe`, `time`, `clock 0`, `module 0|1`, `flow 0|1`, `prog CH HH:MM-HH:MM …`, `mode CH auto|off|man`, `nvsreset`, `status`, `shot`, valamint hálózat: `wifi NÉV|JELSZÓ`, `wififorget`, `portal 1|0`, `otaurl <cím>`, `otacheck`, `net` (állapot, telepítői jelszó, futó programhely), `reboot`. Tesztszkriptek: `tests/ui2_*.txt`, képernyőképek: `build/shots2/`.

## Hálózat, pontos idő, távoli frissítés (firmware 0.4.2, build 7)

### Wi-Fi beállítása telefonról

1. A panelen: Menü → Névjegy → **hosszú nyomás** (telepítői menü) → forgasd a **Hálózat** pontra → nyomás → **Wi-Fi beállítás telefonról**.
2. A kijelzőn megjelenik egy QR-kód, a beállító hálózat neve (`PoolKeeper-71D040`) és a telepítői jelszó.
3. Olvasd be a QR-kódot a telefon kamerájával. A telefon csatlakozik, és magától megnyílik a beállítóoldal; ha mégsem, nyisd meg ezt a címet: `http://192.168.4.1`.
4. Válaszd ki az otthoni Wi-Fi-t, add meg a jelszavát, majd nyomd meg a **Mentés és kapcsolódás** gombot.
5. A panel csatlakozik, és az internetről (NTP, Europe/Vienna) beállítja a pontos időt. A beállító hálózat a sikeres kapcsolódás után 1 perccel, vagy legkésőbb 15 perc múlva magától leáll.

A Wi-Fi adatai a panel NVS-tárában maradnak, nem kerülnek a repóba. Áramszünet után a panel magától visszacsatlakozik: ha a router később indul, 30 másodpercenként újrapróbálja.

### Áramszünet után

- A kimenetek kikapcsolt állapotból indulnak, majd az **AUTO** üzemmódú eszközök az időprogram szerint folytatják, amint a Wi-Fi-n megjön a pontos idő (általában pár másodperc).
- A **KÉZI** állásból 0 lesz, hogy semmi ne induljon el magától kézi bekapcsolással.
- A programok, az üzemmódok, a számlap, a menüsorrend és a Wi-Fi beállítás mind megmarad.

### Azonosító és telepítői jelszó

- Eszközazonosító: **PK-71D040** (a MAC-címből, állandó). Látszik a Névjegyen és a Hálózat oldalon.
- Helyi hálózati név: `poolkeeper-71d040.local`.
- A **telepítői jelszó** véletlenszerű, és első induláskor jön létre. Megtekinthető a telepítői menü Hálózat oldalán. Ez a jelszó védi a beállító hálózatot, a webes beállítást és a helyi frissítést (felhasználónév: `admin`).

### Frissítés távolról (internetről, NAT mögül is)

A panel indulás után kb. 3 perccel, majd **óránként egyszer** lekér egy manifest fájlt HTTPS-en. Ha abban nagyobb `build` szám van, és a frissítés neki szól (`devices`: `*` vagy a saját azonosítója), akkor:

1. letölti a firmware-t a másik programhelyre (A/B partíciók),
2. ellenőrzi az SHA-256 összegét, és eltérés esetén nem telepít,
3. újraindul az új verzióval,
4. ha az új verzió az első 60 másodpercben összeomlik vagy újraindul, a bootloader **automatikusan visszaállítja** az előzőt. Ezt a panelen kipróbáltam.

A kapcsolatot mindig a panel kezdeményezi, ezért nem kell porttovábbítás a routeren.

**Ajánlott tárhely: GitHub Release, automatikus fordítással** (`.github/workflows/firmware-release.yml`):

1. Hozz létre egy GitHub-repót, és töltsd fel ide ezt a projektet. A `.gitignore` kizárja a nagy könyvtárakat és a mentéseket; a felhős fordítás a hivatalos könyvtárakat telepíti, és rájuk másolja a gyári Elecrow-javításokat (`firmware/patches`). Ezt a módszert helyben kipróbáltam, sikeresen fordult.
2. Állítsd be a panelen a manifest címét: a böngészőben nyisd meg a `http://poolkeeper-71d040.local/settings` oldalt (`admin` + telepítői jelszó), és add meg ezt:
   `https://github.com/<FELHASZNÁLÓ>/<REPÓ>/releases/latest/download/manifest.json`
   A címet a soros konzolon is megadhatod: `otaurl <cím>`.
3. Új verzió kiadása, akár laptopról, akár telefonról a Claude Code segítségével:
   - növeld a `PK_FW_BUILD` értékét az [app_config.h](firmware/poolkeeper_ui/src/app_config.h) fájlban;
   - tölts fel egy címkét, például `git tag v0.4.0 && git push --tags`;
   - a GitHub Actions lefordítja a firmware-t, és kiadja a `.bin` fájlt a `manifest.json`-nal együtt. A panel egy órán belül frissít.
   - Csak egy adott panel frissítéséhez az Actions oldalon kézzel is indítható, a `devices` mezőben az azonosítóval (például `PK-71D040`).
4. Azonnali ellenőrzés: Hálózat oldal → **Frissítés keresése most**, vagy a weboldalon a **Frissítés keresése most** gomb.

A repó nyilvánossága: a release-fájlokat a panel bejelentkezés nélkül tölti le, ezért a repónak nyilvánosnak kell lennie. Privát repóhoz külön tárhely kell (például Cloudflare R2 vagy S3), ahová a `scripts/make_ota_release.ps1` kimenetét (`ota/`) kell feltölteni.

**Kézi kiadás saját tárhelyre:** `scripts/make_ota_release.ps1 -BaseUrl https://<tárhely>/poolkeeper`. Ezután az `ota/` mappa két fájlját kell feltölteni.

### Frissítés a helyi hálózaton (laptopról, telefonról)

- **Böngészőből:** `http://poolkeeper-71d040.local/update` (vagy a panel IP-címe) → `admin` + telepítői jelszó → válaszd ki a `.bin` fájlt (`build/poolkeeper_ui/poolkeeper_ui.ino.bin`). A visszaállás-védelem itt is működik.
- **ArduinoOTA:** `poolkeeper-71d040.local`, ugyanazzal a jelszóval.
- **USB:** `scripts/upload.ps1`, ahogy eddig.

### Ami ki van próbálva, és ami még nincs

- ✅ A/B partíciós tábla; a beállítások a táblacsere után is megmaradtak.
- ✅ Beállító portál PC-ről: a weboldal, a Wi-Fi lista és a telefonok felugró oldala (átirányítás) működik.
- ✅ Webes firmware-feltöltés, újraindulás az új programhelyről, érvényesítés 60 s után, és **automatikus visszaállás** szimulált összeomláskor.
- ✅ Üzemmódok visszaállítása újraindulás után (AUTO marad, KÉZI → 0).
- ✅ A felhős fordítás módszere (hivatalos könyvtárak + gyári javítások) helyben lefordult.
- ✅ **Élőben kipróbálva (2026-11-14, firmware 0.4.2):** csatlakozás az otthoni Wi-Fi-hez, pontos idő NTP-ről, és frissítés az internetről. A GitHub Release-ből a panel letöltötte a build 7-et, ellenőrizte, telepítette (app0 → app1), újraindult, majd érvényesnek jelölte.
- ✅ A GitHub-repó (`Zeuszmagus/poolkeeper-crowpanel`, nyilvános) és a GitHub Actions felhős fordítás működik, egy kiadás kb. 4 perc.
- ⚠️ Az ESP32 core 3.3.8 a HTTPS-átirányítás után elveszíti a tanúsítványcsomagot. Ezért a firmware az átirányításokat maga követi, minden lépésben új klienssel ([net/ota.cpp](firmware/poolkeeper_ui/src/net/ota.cpp)). A v0.4.0 és a v0.4.1 kiadás emiatt nem tud frissíteni, ezeket ne telepítsd.

## Kimeneti relémodul (firmware 0.5.0, build 8)

**Modul:** Waveshare Modbus RTU Relay 4CH (RS485, 9600 8N1, 1-es cím). **Tápja:** 7–36 V DC, külön tápegységről.

**Bekötés:** a panel 4 tűs UART-csatlakozója (J34, ZX-MX 1.25 mm) → automatikus irányváltós TTL–RS485 átalakító → a modul A/B kapcsa.

| J34 tű | Jel | Átalakító |
|---|---|---|
| 1 | RX (GPIO44, 5 V-os szintre illesztve) | TXD |
| 2 | TX (GPIO43) | RXD |
| 3 | 5 V (GPIO2 kapcsolja, a firmware bekapcsolja) | VCC |
| 4 | GND | GND |

Az átalakító A+ kapcsát a modul A kapcsára, a B− kapcsát a modul B kapcsára kell kötni. Hosszabb kábelnél a busz végére 120 Ω-os lezáróellenállás kell.

**Relékiosztás** (`PK_RELAY_OF`, [app_config.h](firmware/poolkeeper_ui/src/app_config.h)):

| Relé | Eszköz |
|---|---|
| 1 | Szivattyú |
| 2 | Sóbontó |
| 3 | Világítás |
| 4 | Masszázs |

A relé nélküli eszközök (Nyakzuhany, Ellenáramoltató, Levegőmasszázs) rejtve vannak.

**Működés:**
- Induláskor minden relé kikapcsol.
- A firmware 250 ms-onként visszaolvassa a relék állapotát (FC 01), és eltérés esetén kiírja a kívánt állapotot (FC 0F).
- A kijelző mindig a visszaolvasott állapotot mutatja.
- Ha a modul 5 másodpercig nem válaszol, a kezdőképernyőn megjelenik „A kimeneti modul nem válaszol” üzenet, és a bekapcsolt vagy bekapcsolandó eszközök HIBA állapotba kerülnek.

**Keringés:** áramlásérzékelő nincs. A keringés akkor számít igazoltnak, ha a szivattyúrelé visszaolvasva legalább 30 másodperce be van kapcsolva (`PK_FLOW_CONFIRM_MS`). A sóbontó csak igazolt keringés mellett kapcsol be.

**Konzol:**
- `relay`: kapcsolat, relé-állapot, hibaszámláló és a modul szoftververziója;
- `out sim` / `out modbus`: váltás a szimuláció és a relémodul között.

**Élő teszt (2026-11-17):**
- A modul az első csatlakoztatásra válaszolt (V3.00).
- Mind a négy relé kapcsolt, a visszaolvasott állapot egyezett, a kattogást és a LED-eket a helyszínen ellenőriztük.
- A sóbontó szivattyú nélkül nem kapcsolt be, járó szivattyúval kb. 30 mp után igen.
- A teszt alatt egyetlen kommunikációs hiba sem volt.

**230 V:** a szivattyúmotort mágneskapcsolón keresztül, villanyszerelővel kell bekötni.

### Döntésre vár (Zsolti)

1. Hány másodperc tétlenség után térjen vissza a panel az órához? (Most 60 s, szerkesztőben 180 s.)
2. Hol lehessen beállítani a dátumot és az időzónát? Most a dátum a firmware fordítási napja, ha az óra még nem volt beállítva, az időzóna fix Europe/Vienna.
3. A számlapváltás helye: maradjon a hosszú nyomás az Óra menüponton, vagy legyen külön menüpont?
4. **Áramszünet utáni viselkedés:** 2026-11-14-től megoldva, lásd a „Hálózat, pontos idő, távoli frissítés” fejezetet. Az AUTO üzemmód visszaáll, és az időprogram fut tovább, amint a Wi-Fi-n megjön a pontos idő. A KÉZI üzemmód biztonsági okból 0-ra áll vissza (`PK_RESTORE_MANUAL`). Wi-Fi nélkül a panel továbbra is „Állítsd be az órát” állapotban indul; erre egy elemes RTC lehet a megoldás.
5. Lehessen-e érintéssel üzemmódot váltani? Most nem lehet, csak a gyűrűvel.
6. Az átnevezés a telepítői menüben: kell-e szövegbevitel, vagy előre megadott nevek közül lehessen választani?
7. Az ügyfélszolgálati telefonszám és e-mail-cím (`PK_SUPPORT_PHONE` / `PK_SUPPORT_EMAIL`, [app_config.h](firmware/poolkeeper_ui/src/app_config.h)).

## Hardver (azonosítva 2026-11-12)

| | |
|---|---|
| Panel | CrowPanel 1.46″ HMI ESP32 Rotary Display (DHR55146D), HW V1.0 |
| SoC | ESP32-S3 (QFN56) rev v0.2, 240 MHz, 8 MB beágyazott PSRAM (OPI) |
| Flash | 16 MB (gyártó 0xBA, eszköz 0x4018), quad, 3,3 V |
| MAC | 44:bd:8d:71:d0:40 |
| USB | natív USB-Serial/JTAG, VID 303A PID 1001 → **COM10** ezen a gépen |
| Kijelző | ST77961 360×360, 3-vezetékes SPI (SCLK 10, MOSI 11, CS 9, DC 3, RST 14), háttérfény GPIO46 |
| Érintés | CST816T, I²C SDA 6 / SCL 7, RST 13, INT 5 |
| Gyűrű | enkóder A 45 / B 42, nyomógomb 41 (aktív alacsony) |
| Egyéb | tápsínek GPIO1/2, táp-LED 40, 8× WS2812 (48, táp 17), külső I²C 38/39 |

## Rögzített verziók

| Összetevő | Verzió | Megjegyzés |
|---|---|---|
| arduino-cli | 1.6.0 | portable, `tools/arduino-cli/` |
| Arduino ESP32 core | **3.3.8** | a gyári firmware (2026-09-16) „3.3.8-cn” alapja. A wiki és a régi readme még 2.0.14-et ír, de a jelenlegi gyári forrás (`RotaryScreen_1_46_Code_Core3_LVGL9`) core 3-at és LVGL 9-et használ. |
| LVGL | 9.1.0 | a gyári csomagból, `firmware/libraries/lvgl` |
| LovyanGFX | 1.2.7 | gyári csomag |
| cst816t | 1.5.1 | gyári csomag |
| Adafruit NeoPixel | 1.15.1 | gyári csomag |
| lv_font_conv | 1.5.2 | magyar fontok generálása |
| esptool | 5.6.0 | `tools/venv` (Python 3.13) |
| Gyári repó | Elecrow-RD/CrowPanel-1.46inch-HMI-ESP32-Rotary-Display @ `3dfdbfe` (2026-10-20) | `vendor/elecrow-1.46` |

Board (FQBN): `esp32:esp32:esp32s3:CDCOnBoot=cdc,USBMode=hwcdc,FlashMode=dio,FlashSize=16M,PSRAM=opi,PartitionScheme=custom,CPUFreq=240,LoopCore=1,EventsCore=1,DebugLevel=none`.
Partíciós tábla: `firmware/poolkeeper_panel/partitions.csv`. Ez a gyári `elecrow_s3` (10 MB APP), a lefordított `partitions.bin` bájtra azonos a gyárival.

## Telepítés nulláról (Windows)

Előfeltétel: Git, Python 3.13, Node.js 22. A szkriptek PowerShellből és Git Bash-ből futnak.

```powershell
# 1) gyári repó
git clone --depth 1 https://github.com/Elecrow-RD/CrowPanel-1.46inch-HMI-ESP32-Rotary-Display vendor/elecrow-1.46
# 2) arduino-cli 1.6.0 (Windows_64bit.zip) kicsomagolva ide: tools/arduino-cli/
#    a konfiguráció (tools/arduino-cli.yaml) mindent a projektmappába tesz, a felhasználói Arduino mappát nem érinti
tools/arduino-cli/arduino-cli.exe --config-file tools/arduino-cli.yaml core update-index
tools/arduino-cli/arduino-cli.exe --config-file tools/arduino-cli.yaml core install esp32:esp32@3.3.8
# 3) Python eszközök
python -m venv tools/venv
tools/venv/Scripts/python.exe -m pip install esptool pyserial pillow
# 4) (csak fontmódosításkor) fontkonverter
cd tools/fontconv; npm install lv_font_conv@1.5.2; cd ../..
```

A panel a hátoldali **USB** feliratú aljzatra csatlakozik, adatkábellel. Az Eszközkezelőben „Soros USB-eszköz (COMx)” néven jelenik meg, VID_303A azonosítóval. Külön driver nem kell.

## Napi használat

| Feladat | Parancs (VS Code: *Terminal → Run Task*) |
|---|---|
| Fordítás | `powershell -ExecutionPolicy Bypass -File scripts\build.ps1` |
| Fordítás + feltöltés | `powershell -ExecutionPolicy Bypass -File scripts\upload.ps1` (a portot magától megkeresi; `-Port COM10` és `-NoBuild` is megadható) |
| Soros monitor | `powershell -ExecutionPolicy Bypass -File scripts\monitor.ps1` (115200, kilépés: Ctrl+C) |
| Képernyőkép | `tools\venv\Scripts\python.exe scripts\pk_serial.py shot build\shots\screen.png` |
| Parancsfájl futtatása | `tools\venv\Scripts\python.exe scripts\pk_serial.py script tests\ui_tour_full.txt` |
| Fontok újragenerálása | `bash scripts/gen_fonts.sh` |

### Helyreállítás (BOOT/RESET)

- Ha a feltöltés „Connecting...”-nél elakad, vagy a firmware összeomlik és a COM port eltűnik: **tartsd lenyomva a BOOT gombot, nyomd meg röviden a RESET gombot, majd engedd el a BOOT gombot.** A chip ROM letöltési módba lép, és a port újra megjelenik. Ezután töltsd fel újra a firmware-t, majd nyomj RESET-et.
- Instabil kapcsolatnál próbáld 115200 baudon (`esptool --baud 115200`), és zárd be a soros monitort.

### Gyári állapot visszaállítása

```powershell
powershell -ExecutionPolicy Bypass -File scripts\restore_factory.ps1          # saját panel mentése (16 MB)
powershell -ExecutionPolicy Bypass -File scripts\restore_factory.ps1 -Vendor  # Elecrow 2026-09-16 gyári kép
```

A `backup/panel_44bd8d71d040_flash_16MB_20261112.bin` a panel eredeti, teljes flash-tartalma (SHA-256 `bced47d6…3968`). Ez **régebbi** gyári app, mint a repóban lévő 2026-09-16-os kiadás: a partíciós tábla azonos, a bootloader és az app eltér. A szkript írás előtt ellenőrzi a SHA-256 összeget. A mentés nem kerül a Gitbe, mert az NVS-ben Wi-Fi adatot tárolhat.

## Firmware felépítése

```
firmware/
  libraries/           rögzített könyvtárak + lv_conf.h (LVGL 9.1)
  poolkeeper_panel/
    poolkeeper_panel.ino   setup/loop
    partitions.csv         elecrow_s3 (10 MB APP)
    src/app_config.h       időzóna, időkorlátok, verzió
    src/hal/               kijelző, érintés, gyűrű (2 ms-os task → eseménysor), LED-gyűrű
    src/model/             állapot + heti időzítés (éjfélátlépés, DST)
    src/backend/           vezérlési interfész; M1-ben SimBackend mindkét profilhoz
    src/ui/                LVGL nézetek (közös Basic/Full)
    src/diag/console.cpp   soros diagnosztikai konzol
    src/fonts/             generált magyar fontok (Montserrat OFL + FontAwesome szimbólumok)
scripts/               build / upload / monitor / restore / pk_serial.py / gen_fonts.sh
tests/                 soros parancsfájlok a készüléken futó próbákhoz
```

Az LVGL-t csak a `loop()` hívja. A gyűrűt egy külön task mintavételezi, és csak eseményeket küld egy sorba. A nézetréteg a `Backend` interfészen keresztül kér konkrét célállapotot (BE/KI). A visszaigazolt állapotot mindig a háttér adja: Basic profilban a kimeneti modul, Full profilban a Pi. Full módban kapcsolatvesztéskor a panel nem kapcsol önállóan.

## Kezelés

- **Lapozó mód:** a gyűrű forgatása oldalt vált (Medence → [Vízértékek] → Gépészet → Időzítés → Rendszer), oldalra húzással is lehet lapozni. A **nyomás** belép az oldal kezelésébe, a **hosszú vagy dupla nyomás** visszavisz a Medence nézetre.
- **Kezelő mód:** a forgatás kijelöl, a nyomás vagy az érintés megerősít, a **dupla nyomás** visszalép.
  - *Gépészet:* a forgatás kimenetet választ, a nyomás a másik állapotot kéri. A kijelző külön mutatja a kért állapotot (sárga, várakozik), a visszaigazolást (zöld) és a hibát vagy lejáratot (piros). A hosszú nyomás visszaállítja az automata módot. A fizikai működést (pl. „Áramlás: van”) csak visszajelző bemenettel rendelkező kimenetnél írja ki.
  - *Időzítés:* kimenet, napok, kezdés és vége (15 perces henger), Mentés. A mentetlen módosítást a sárga „Mentés *” jelzi, mentés nélküli kilépéskor az érvényes program töltődik vissza. Kézi felülbíráláskor megjelenik a „Vissza automatára” gomb.
- LED-gyűrű: borostyán = kapcsolat- vagy órahiba, sárga = függő parancs.

### Soros konzol (115200)

`help`, `status`, `shot`, `r [n]` / `l [n]` (forgatás), `c` / `d` / `L` (nyomás / dupla / hosszú), `tap X Y`, `swipe X0 Y0 X1 Y1`, `page N`, `link 0|1`, `fault 0|1`, `clock 0|1`, `time YYYY-MM-DD HH:MM`, `sched I DAYSHEX HH:MM HH:MM`, `profile basic|full`.

## Állapot (2026-11-12)

### M0 — Windows és gyári demó

- [x] Board- és könyvtárverziók rögzítve (lásd fent).
- [x] A gyári `RotaryScreen_1_46_Code_Core3_LVGL9` sketch lefordul ugyanezzel a toolchainnel (4,0 MB, ezért a 10 MB-os gyári partíció szükséges).
- [x] USB-feltöltés esptool-lal (hash ellenőrizve), soros log az USB-Serial/JTAG porton.
- [x] A panel eredeti flash-tartalma mentve és ellenőrizve, a visszaállító szkript elkészült.
- [ ] **Függő:** a kijelző, a forgatás, a benyomás és az érintés fizikai ellenőrzése (szemmel / kézzel, lásd lent).

### M1 — PoolKeeper UI szimulált adatokkal

- [x] 360×360-as magyar felület (ő/ű ékezetek, tizedesvessző), Basic és Full profil, közös nézetréteg.
- [x] Szimulált friss adat, függő kapcsolás és visszaigazolás, reteszelutasítás (fűtés keringetés nélkül), parancslejárat kapcsolatvesztéskor, elavult adat (30 s demó-küszöb), szenzorhiba, érvénytelen óra, kapcsolat nélküli mentés elutasítása.
- [x] Minden képernyő ellenőrizve a panelről soros vonalon lekért képernyőképekkel (`build/shots/`).
- [x] Időzítéslogika a készüléken ellenőrizve: éjfélen átnyúló ablak (az előző naphoz tartozik), tavaszi DST (a nem létező 02:30 → 03:00 CEST), őszi DST (`tests/schedule_logic.txt`).
- [ ] **Függő:** olvashatóság és színek a fizikai panelen, valamint a valós gyűrű- és érintésirány.

### Nyitott kérdések / következő lépések

- **Őszi óraátállítás:** az ismételt órában (02:00–03:00) a 02:00–02:30 közti szakasz kétszer telik el, így egy 01:00–02:30 ablak 02:00 CET-kor újra bekapcsol 30 percre. Az M2 előtt dönteni kell, hogy ez elfogadható-e, vagy az ablak egyszer fusson le.
- M2: NVS-be mentés (verziózott séma), NTP és elemes RTC, a külső kimeneti modul lábkiosztása a kapcsolási rajz (`vendor/elecrow-1.46/Eagle_SCH&PCB`) alapján, a szimulált óra kiváltása.
- M3: helyi MQTT-protokoll a Pi-vel (verzió, eszközazonosító, parancsazonosító, lejárat, nem retained parancsok).
- A meglévő PoolKeeper repók (Pi, Flutter, Firebase) nincsenek ezen a gépen. A projektterv 1. lépése (felmérés) ezért még nyitott.
- PC-s LVGL szimulátorhoz C fordító (MinGW vagy MSVC) és SDL2 kell, ezek nincsenek telepítve. Helyettük a soros képernyőkép-lekérés szolgál vizuális ellenőrzésre a valós panelről.
