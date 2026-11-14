# PoolKeeper CrowPanel — ügyfélfelület

Élő tervezési jegyzet Zsoltival. A jóváhagyott ötleteket lépésenként ide írjuk, a végleges tervet egyben adjuk át Claude Code-nak a VS Code-os megvalósításhoz.

Célhardver: Elecrow CrowPanel DHR55146D, kerek 360 × 360-as érintőkijelző, egy forgatható és benyomható kezelőszerv. A kezelés könnyen megjegyezhető: forgatás a kijelöléshez vagy az érték módosításához, benyomás a kiválasztáshoz, érintés a közvetlen művelethez. Minden fontos ügyfélművelet legyen elérhető a forgatóval és benyomással is.

## 01 — Óra, menünyitás és időbeállítás

### Zsolti elképzelése

- A gyári alapnézet egy nagy, első pillantásra leolvasható analóg óra, igényes karóra-számlap hangulatával. A számlap középső részén kis, visszafogott PoolKeeper márkafelirat jelenjen meg. A mutatók és az osztások maradjanak jól olvashatók.
- Két választható megjelenés legyen: látványos analóg számlap és nagy számjegyes digitális óra.
- Az óra nézetéből a kijelző érintése vagy a gomb rövid benyomása nyissa meg a menüt.
- A menüből legyen elérhető az óra megjelenésének és a pontos időnek a beállítása.

### Javasolt viselkedés a közös tervezéshez

- Analóg nézet: kontrasztos mutatók, jól látható fő osztások, a kerek kijelzőhöz igazított számlap. Digitális nézet: nagy, 24 órás HH:MM kijelzés.
- A menüt megnyitó érintés vagy benyomás csak a menüt nyissa meg. A megnyitó mozdulat ne aktiválja az első menüpontot.
- A kijelzés módját mentse a panel. Érvénytelen óra esetén jelenjen meg az „Állítsd be az órát” üzenet. Az érvénytelen idővel az időzítő viselkedését a vezérlési terv rögzíti.
- Basic módban az időforrás a panel helyi órája. Full módban a Pi által használt idő legyen a mérvadó. A Full módú kézi időállítás jogosultságát és megvalósítását külön pontosítjuk.

### Nyitott döntések

- Mennyi tétlenség után térjen vissza a panel az órához.
- A dátum és az időzóna beállításának helye.

## 02 — Lapozás a főmenüben

- A menüpontok között kétféleképpen lehet lapozni: a gomb forgatásával vagy a kijelzőn vízszintesen balra, illetve jobbra húzva az ujjat.
- A kerek kijelző két oldalán kis, jól látható nyíl jelzi a lapozást. Balra a balra mutató, jobbra a jobbra mutató nyíl jelenjen meg.
- Javasolt működés: középen egyszerre egy menüpont neve látszik nagy betűvel. Jobbra forgatva vagy balra húzva a következő menüpont jelenik meg. Balra forgatva vagy jobbra húzva az előző. A gomb benyomása megnyitja az éppen látható menüpontot. A húzás csak lapozzon, így a mozdulat felengedése nem nyit meg menüpontot.

## 03 — Pontos idő beállítása

1. Az **Óra** menüpontnál a gomb benyomása megnyitja az órabeállítást. Középen nagy számjegyekkel csak az óra értéke jelenik meg (00–23).
2. Felfelé húzás vagy jobbra forgatás növeli az értéket. Lefelé húzás vagy balra forgatás csökkenti.
3. A következő gombnyomás rögzíti a választott órát a szerkesztéshez, és átvált a perc képernyőjére. Itt nagy számjegyekkel csak a perc értéke jelenik meg (00–59). A mozdulatok ugyanúgy működnek.
4. A következő gombnyomás rögzíti a percet, érvényesíti a teljes új időt, és visszavisz a menü **Óra** pontjára.

Az első megerősítéskor az óra értéke ideiglenesen mentődik. A futó óra és az időzítés a perc megerősítésekor kapja meg együtt az új órát és percet. A kétlépéses folyamatban a gombnyomás maga a megerősítés.

## 04 — Szivattyú

A főmenüben az **Óra** után a **Szivattyú** következik. A gomb benyomása belép a szivattyú menüjébe, és megmutatja az aktuális üzemmódot. A menü megnyitása önmagában nem kapcsolja a szivattyút.

| Aktuális mód | Balra fordítás | Jobbra fordítás |
|---|---|---|
| **AUTO — Időzített** | AUTO marad | **0 — Ki**, leáll |
| **0 — Ki** | **AUTO — Időzített**, a beállított időprogram veszi át a vezérlést | **KÉZI — Be**, azonnali indítás |
| **KÉZI — Be** | **0 — Ki**, azonnali leállítás | KÉZI marad |

- A **0** állapotban a képernyő bal oldalán **AUTO**, jobb oldalán **KÉZI BE** mutatja a választható irányokat. A KÉZI állapotból egy balra fordítás csak a **0** állapotig lép. Az AUTO külön balra fordítással választható.
- **KÉZI — Be** módban a szivattyú folyamatosan megy, amíg a kezelő vissza nem fordítja **0 — Ki** állásba. A kézi járatásnak nincs időkorlátja.
- **AUTO** módban az üzemmód mellett külön jelenjen meg, hogy az időprogram éppen járatja-e a szivattyút. A kijelző a kapcsolás eredményét a vezérlő visszajelzése alapján mutassa.
- AUTO állásból egy jobbra fordítás 0 — Ki állásba visz, és leállítja a szivattyút. Az érintéses váltást és az áramszünet utáni viselkedést külön pontosítjuk.

## 05 — Napi szivattyú-időzítés

A 0 — Ki állásból balra fordítva lépünk AUTO módba, a 04. pontban jóváhagyott kiosztás szerint. AUTO módban a gomb rövid megnyomása nyitja meg az időprogramot.

### Jóváhagyott első változat

- Ugyanaz a program ismétlődik minden nap. Egy napon belül legfeljebb öt BE–KI időszak adható meg.
- Az időprogram áttekintése egy 24 órás körszámlap, amely a bekapcsolási időszakokat színes ívekkel mutatja. A kiválasztott időszak középen számmal is olvasható, például **08:00–10:30**.
- A forgatóval választható a meglévő időszak, az **Új időszak** vagy a **Mentés**. Üres programnál az **Új időszak** legyen az első lehetőség. Csak a kitöltött időszakok jelenjenek meg. Öt időszaknál az **Új időszak** eltűnik.
- Szerkesztési sorrend **BE óra → BE perc → KI óra → KI perc**. Képernyőnként egy nagy szám és rövid mezőfelirat jelenjen meg. Jobbra forgatás vagy felfelé húzás növeli az értéket. Balra forgatás vagy lefelé húzás csökkenti. A perc egyperces lépésekben állítható (00–59), a BE és a KI időpontnál egyaránt.
- A gombnyomás a következő mezőre lép. A KI perc jóváhagyása után érvényes időszaknál visszatérünk az áttekintéshez, ahol a **Mentés** legyen kijelölve. Egy újabb gombnyomás vagy a Mentés megérintése véglegesíti a teljes napi programot. Új időszakhoz a forgatóval válaszd az **Új időszak** pontot. Így például három időszak után rögtön menthető a program.
- Új időszak felvételekor a BE óra és perc az utoljára beállított teljes időszak KI idejéről indul. Ez szerkeszthető kezdőérték, amelyet a felhasználó megerősít. Az első időszaknál nincs korábbi KI érték.
- A hiányos BE–KI pár és a **KI ≤ BE** érvénytelen. Ilyenkor maradjon elérhető a javítás, és jelenjen meg az ok: „Add meg a be- és kikapcsolás idejét” vagy „A kikapcsolás ideje legyen későbbi”. Hibás időszakkal a teljes program sem menthető. Az első változatban az éjfélen átnyúló egybefüggő időszak nem menthető.
- A meglévő időszak törölhető. A szivattyút a korábban elmentett program vezérli, amíg az ügyfél a **Mentés** művelettel véglegesíti a szerkesztést.
- A 24 órás gyűrű áttekintést ad, a pontos időszak a középen megjelenő számokból olvasható le. Az időgyűrű közvetlen érintéses szerkesztése későbbi, fizikai panelen kipróbálandó lehetőség.

## 06 — További vezérlések

A főmenü kiinduló sorrendje **Óra → Szivattyú → Világítás → Nyakzuhany → Masszázs → Ellenáramoltató → Levegőmasszázs → Sóbontó → Statisztika → Névjegy**.

A Világítás, Nyakzuhany, Masszázs, Ellenáramoltató és Levegőmasszázs külön menüpont és külön vezérlési csatorna. Mindegyik újrahasználja a Szivattyú háromállású kezelését (04) és a napi időzítés szerkesztését (05).

- Belépéskor az adott csatorna aktuális módja és állapota látható. **0 — Ki** állásból balra **AUTO**, jobbra **KÉZI BE**. A közép felé egy lépéssel visszaáll **0 — Ki** állásba. KÉZI BE módban az adott funkció azonnal bekapcsol, és a kezelő kikapcsolásáig bekapcsolva marad.
- AUTO módban a gombnyomás az adott csatorna időprogramját nyitja meg. Csatornánként legfeljebb öt napi BE–KI időszak menthető, egyperces pontossággal. Ugyanaz a program fut minden nap. A korábbi KI időből induló új BE idő, a külön Mentés és az időpárok hibakezelése a 05. pont szerinti.
- Az üzemmód, a mentett program és a kimenet állapota csatornánként külön kezelendő. Az egyik csatorna szerkesztése vagy kapcsolása a saját beállítására vonatkozik.

A hét vezérléshez teljes kiépítésben hét önálló vezérlőcsatorna kell. Négycsatornás modullal a ténylegesen bekötött négy csatorna konfigurálható.

## 07 — Statisztika 

Javasolt első tartalom a szivattyú mai, tegnapi és összes üzemideje. A számlálás alapja a rendelkezésre álló visszajelzés. Ha csak a vezérlőkimenet állapota ismert, a megnevezés **Bekapcsolt kimenet ideje** legyen. A pontos mutatókat közösen véglegesítjük.

## 08 — Névjegy 

A főmenü végén jelenjen meg a **PoolKeeper** márkanév és az ügyfélszolgálati telefonszám és/vagy e-mail-cím. A végleges elérhetőséget Zsolti adja meg.

## 09 — Telepítői beállítások

- Az ügyfélhez előre állítható be, mely eszközök szerepeljenek a menüben, milyen néven és milyen sorrendben. A nem telepített funkciók rejtve maradnak. Az Óra, Statisztika és Névjegy helye alapból rögzített.
- Egy eszköz kiválasztásakor legyen **Név**, **Megjelenítés** és **Áthelyezés**. Áthelyezéskor a forgató mozgatja a menüpontot, a gombnyomás rögzíti a pozíciót. A telepítési profilt a Mentés véglegesíti, és újraindítás után is megmarad.
- A megjelenített név és sorrend független az eszköz állandó azonosítójától, kimenetétől és időprogramjától. Az átnevezés és átrendezés nem kapcsol kimenetet. Az elrejtés megjelenítési beállítás, a csatorna üzemen kívül helyezése külön művelet.

## 10 — Sóbontó

- Saját menüpont, önálló **AUTO / 0 — KI / KÉZI BE** állással. Az AUTO mód programja a szivattyú mentett napi ciklusaihoz igazodik.
- A KÉZI BE és az AUTO egyaránt működési kérést ad. A tényleges indítás feltételeit a konkrét sóbontó gyártói előírásai határozzák meg. A sóbontó csak igazolt keringés mellett működhessen, a gyári áramlásvédelmet meg kell tartani. A sóbontó időprogramja önmagában nem indítja el a szivattyút.
- Ha a program vagy a kezelő működést kér, de nincs igazolt keringés, jelenjen meg a **Keringésre vár** állapot. Keringésvesztéskor a vezérlés vonja vissza a működési kérést. A pontos vezérlőbemenet és visszajelzés a kiválasztott készülék függvénye.

### Sóbontás a kiválasztott keringetési ciklusokban

- AUTO módban gombnyomásra a szivattyú mentett ciklusai jelennek meg. Ciklusonként választható a **Kihagyás**, a **Teljes ciklus** vagy a **Rövidebb időszak**. Új ciklus alapból kihagyott.
- Rövidebb időszaknál a ciklus határain belül külön BE és KI idő állítható, egyperces pontossággal, a 05. pont kezelési módjával. A KI idő legyen későbbi a BE időnél. Példa három keringetési ciklusból csak a 12:00–14:00 ciklusban, azon belül 12:15–13:00 között kérünk sóbontást.
- A működési kéréshez egyszerre kell teljesülnie a kiválasztott időablaknak és az igazolt keringésnek. Késői indulás vagy áramláskiesés miatt az időablak ne tolódjon ki. Kézi szivattyújáratás önmagában nem hosszabbítja meg az AUTO sóbontás időablakát.
- A választás stabil ciklusazonosítóhoz tartozzon. Ha a szivattyúciklust törlik vagy úgy módosítják, hogy a beállított rövidebb időszak kilóg belőle, az érintett sóbontó-beállítás inaktiválódjon, és jelenjen meg az „Időzítés ellenőrzése” jelzés.

## 11 — Színek, körgrafika és olvashatóság

- A PoolKeeper alap arculata kék és sárga. Javasolt kiindulás a mélykék háttér, világos számok és feliratok, valamint sárga kiemelés az aktív kijelöléshez. Az egyes menük kaphatnak saját kiegészítő színt, az állapotjelzések jelentése minden menüben egységes maradjon.
- Használjunk színes köríveket, gyűrűket és rovátkákat. Az órán a számlap osztásait, az időzítőn a bekapcsolási időszakokat, a vezérléseknél az aktuális módot emelhetik ki. Az AUTO, KI, KÉZI BE és hibajelzések szöveggel vagy egyértelmű ikonnal is azonosíthatók legyenek.
- Menüváltáskor használható rövid, finom oldalirányú átmenet, amely követi a lapozás irányát. Az animáció közben is reagáljon a panel a kezelőszervekre. A kapcsolási visszajelzés legyen azonnal olvasható.
- A 360 × 360-as kerek kijelzőn a fontos értékek és feliratok nagy, kontrasztos betűkkel jelenjenek meg. A széleken a nyilak és rovátkák mellett is maradjon hely. A magyar ékezeteket, a hosszabb eszközneveket és az olvashatóságot a fizikai panelen is ellenőrizzük.
