# PoolKeeper CrowPanel — projektterv a VS Code-ban dolgozó kódoló ügynöknek

A cél egy közös, magyar nyelvű kijelző-firmware az Elecrow CrowPanel 1,46″ panelre, két telepítési profillal. A mechanikai rögzítés külön feladat. A jelen dokumentum fejlesztési terv, a panelen való működést helyben kell igazolni.

## 1. Két telepítési profil

| Funkció | Basic — CrowPanel önállóan | Full — CrowPanel + Raspberry Pi + Flutter |
|---|---|---|
| Vezérlés | A panel futtatja az ütemezést és vezérli a külső kimeneti modult | A Raspberry Pi 3B+ az egyetlen vezérlő, a panel kezelőfelület |
| Kimenetek | Első körben szűrés és világítás, kézi és automata mód | Szivattyú, fűtés, világítás és élményelemek a meglévő konfiguráció szerint |
| Időzítés | Helyben mentett heti program, internet nélkül is érvényes órával | A Pi helyi ütemezése, állítható a panelről és a jogosult Flutter-fiókból |
| Vízértékek | A kezdőképernyő a szűrés és a következő kapcsolás állapotát mutatja | Atlas EZO pH, ORP, hőmérséklet és a már támogatott további mérések, frissességjelzéssel |
| Mobilapp | A Basic első kiadása helyi kezelést ad | Meglévő Flutter és Firebase integráció, távoli állapot és vezérlés |
| Kapcsolatvesztés | Az ütemezés érvényes helyi órával tovább fut | A Pi tovább vezérel, a panel és az app jelzi az elavult adatot |

A két profil ugyanazt az LVGL nézetréteget használja, eltérő vezérlési háttérrel. A profil telepítői beállítás. Full módban kapcsolatvesztéskor a panel nem válthat önálló relévezérlésre. Basic → Full váltáskor az időzítéseket külön ellenőrizni és átvinni kell, a Basic kimeneti vezérlést le kell választani az átadás előtt.

## 2. Első feladat a Windowsos VS Code munkaterületen

1. Nyisd meg a meglévő PoolKeeper mappát. Térképezd fel a firmware, Pi és Flutter repókat, a Git-állapotot, az AGENTS.md fájlokat, az aktuális Pi szolgáltatást és a Firebase adatútvonalakat. A meglévő kód és szerződések legyenek a kiindulópont.
2. Hardverreferencia a 60 mm-es hátlapú DHR55146D, 360 × 360 pixeles kerek IPS érintőkijelző, ESP32-S3. Ellenőrizd a gyári mintát és a hardverrevíziót. A hátoldali USB feliratú aljzathoz a mellékelt USB-adatkábelt használd. Ellenőrizd a Windows Eszközkezelőben, hogy megjelenik-e az eszköz vagy a COM port.
3. A gyári demón próbáld ki a kijelzőt, érintést, forgatást és benyomást. Fordítsd le a megfelelő Elecrow Arduino példát. A gyártó wiki kiinduló ajánlása ehhez a modellhez az ESP32 Arduino 2.0.14. A ténylegesen használt panelbeállításokat és a gyári példához tartozó LVGL és driver verziókat rögzítsd. Őrizd meg a visszaállítható gyári firmware-t és a hozzá tartozó gyári feltöltési utasítást, ha elérhető.
4. Arduino IDE 2-vel igazold az első feltöltést. Utána készíts reprodukálható VS Code buildet Arduino CLI-vel vagy a meglévő firmware-buildrendszerrel. Dokumentáld a telepítést, a pontos verziókat, a build és upload parancsot, a soros monitor használatát és a BOOT/RESET helyreállítást. PlatformIO-ra akkor térj át, ha ugyanazt a hardverpróbát sikerül vele megismételni.

Gyári források
- [Elecrow 1,46″ wiki](https://www.elecrow.com/wiki/CrowPanel_1.46-inch-HMI_ESP32_Rotary_Display_360x360_IPS_Round_Touch_Knob_Screen.html)
- [1,46″ mintakód és gyári fájlok](https://github.com/Elecrow-RD/CrowPanel-1.46inch-HMI-ESP32-Rotary-Display)

## 3. Közös kijelző és szimulátor

- Medence nézet. Nagy vízhőmérséklet Full módban. Basic módban nagy szűrési állapot és a következő kapcsolás ideje. Sötét háttér, türkiz állapotív, nagy kontraszt, magyar ékezetek.
- Gépészet. A gyűrűvel egyenként választható kimenetek. A kijelző külön jelzi a kért műveletet, a vezérlő visszaigazolását és a hibát. Fizikai működést csak akkor írjon ki, ha van hozzá visszajelző bemenet.
- Időzítés. Napok, kezdés és befejezés, valamint jól látható mentés. Kézi felülbírálás lejárattal és visszatérés automata módba. A forgatás kijelöl, a benyomás vagy érintés megerősít.
- Vízértékek Full módban. pH, ORP és vízhőmérséklet értékkel, mértékegységgel és utolsó frissítéssel. Hiányzó vagy elavult adatnál egyértelmű állapotjelzés.
- A kör széleit hagyd szabadon a hosszú feliratoknál. Az első demó szimuláljon friss adatot, függő kapcsolást és kapcsolatvesztést. SquareLine használata opcionális. A gyári LVGL verzióval kompatibilis exportot és az ügyféltermékhez megfelelő licencet ellenőrizd.

## 4. Basic működés Raspberry nélkül

- Külön, dokumentált 5 V tápellátás és a panelhez illesztett külső kimeneti modul szükséges. A rendelkezésre álló GPIO, UART vagy I²C lehetőségeket a konkrét kapcsolási rajz és a gyári driver kiosztása alapján ellenőrizd. A csatlakozó 5 V tápjelölése önmagában nem igazolja a jelszintet. Első hardverpróba USB-ről, szimulált kimenettel vagy kisfeszültségű LED-terheléssel.
- Az ütemezést, az időzónát és a kimeneti konfigurációt verziózott NVS-be mentse. Az ütemezés heti, kezeli az éjfélen átnyúló időablakot, az újraindítást és a nyári/téli óraátállást. Teszteld az Europe/Vienna időzónát.
- Ha rendelkezésre áll hálózati idő, használható NTP. Ha áramszünet után hálózat nélkül is automatikusan kell indulnia, tervezz kompatibilis, elemes RTC-t. Érvénytelen óránál jelezzen hibát és függessze fel az automata időzítést.
- A kimenetek indulási és hibakezelési állapotát dokumentáld. Prototípusban újrainduláskor minden kimenet kikapcsolt, majd az érvényes idő, mód és aktuális program alapján értékelhető újra. A kézi felülbírálás alapból ne élje túl az újraindulást. A végleges gépészeti terheléshez külön méretezett kapcsoló- és védelmi fokozat kell.

## 5. Full működés a meglévő PoolKeeperrel

- A Pi helyben tartsa az időzítést, a kézi felülbírálást, a reléállapotot és a gépészeti feltételeket. Fűtéshez ellenőrizze a rendelkezésre álló keringési és áramlási visszajelzést. Vegyszeradagolást külön mérföldkőben kezelj az érzékelőhibák és a hardveres reteszek felmérése után.
- A panel helyi MQTT-n csatlakozhat, ha a repó meglévő protokollja nem kínál kész megoldást. A protokollban legyen verzió, eszközazonosító, állapot és időbélyeg, egyedi parancsazonosító, lejárat és visszaigazolás. Küldj konkrét célállapotot, például BE. A kapcsolási parancsok ne legyenek retained üzenetek. Ismételt vagy lejárt parancs ne indítson új kapcsolást.
- A Pi felhőkapcsolata illeszkedjen a meglévő Firebase Auth és Firestore/RTDB szerkezethez. A Flutter app a már működő belépésen és ügyfél–medence hozzárendelésen keresztül lássa a telemetriát és küldjön kérelmet. Távoli parancsoknál ellenőrizd a jogosultságokat, a lejáratot és a Pi visszaigazolását. Hitelesítő adatot ne tárolj a repóban, a helyi MQTT brokert ne publikáld az internetre.
- Internetkimaradáskor maradjon működőképes a helyi panel–Pi kapcsolat és a Pi automatika. Pi- vagy LAN-kieséskor a panel jelezze a kapcsolat hiányát. Visszacsatlakozáskor friss állapotot kérjen, a lejárt parancsokat dobja el. Egyidejű paneles és appos műveletek sorrendjéről a Pi döntsön, majd mindkét felület ugyanazt a visszaigazolt állapotot kapja.

## 6. Mérföldkövek és átadási feltételek

| Szakasz | Ellenőrizhető eredmény |
|---|---|
| M0 — Windows és gyári demó | Rögzített board- és könyvtárverziók, sikeres build, USB-feltöltés és soros log. A kijelző, forgatás, benyomás és érintés fizikailag ellenőrizve. |
| M1 — PoolKeeper UI | 360 × 360-as magyar felület szimulált adatokkal, olvasható a fizikai panelen. Szimulált kapcsolás, hiba és kapcsolatvesztés látható. |
| M2 — Basic prototípus | Két kimenet szimulációval vagy kisfeszültségű próbaterheléssel. Kézi kapcsolás, heti időzítés, áramszünet, érvénytelen óra és éjfélátlépés ellenőrizve. |
| M3 — Full helyi prototípus | Valós vagy szimulált Pi-telemetria a kijelzőn. Kért és visszaigazolt kapcsolás, duplikált parancs, valamint Pi- és hálózati kiesés ellenőrizve. |
| M4 — Flutter integráció | Tesztfiókkal és tesztmedencével működik az app → Pi → panel és panel → Pi → app állapotfrissítés. Jogosulatlan fiók és lejárt parancs próbája is sikeres. |

**Indítsd M0-val és készítsd elő M1-et.** Az első munkamenet végén add meg a felismert repókat, a reprodukálható Windows build lépéseit, az elkészült módosításokat és a futtatott próbák eredményét. Ha az ügynök környezetéből a Windowsos USB-port nem elérhető, készítsd elő a helyi futtatást, és a fizikai feltöltést jelöld függőnek a tényleges próba elvégzéséig.
