# Cosmic Random Interference Visualizer — LilyGo LoRa32 T3_V1.6.1 (433 MHz)

> 🚀 **Tento projekt je 100% VIBECODED!**  
> Veškerý kód, architektura, návrh algoritmů, řešení hardwarových záludností ESP32, optimalizace na 50 FPS i nahrávání firmwaru vznikly výhradně formou dialogu v přirozeném jazyce mezi vývojářem a autonomním AI agentem. Uživatel nenapsal ani řádek kódu ručně.  
> 📜 **Kompletní a nestříhaný přepis naší konverzace najdete v [CONVERSATION.md](CONVERSATION.md).**

---

Program pro desku **LilyGo LoRa32 T3_V1.6.1 (433MHz)**, který snímá náhodné rušení z okolního elektromagnetického prostředí, kosmický šum a rádiové signály v pásmu 433 MHz pomocí transceiveru SX1278 a ESP32 hardware TRNG/ADC.

Na OLED displeji SSD1306 (128×64) nabízí **5 přepínatelných vizualizačních režimů**.

---

## 🤖 Vibecoding & Použité AI nástroje a modely

Projekt byl vytvořen od nuly bez manuálního psaní kódu pomocí agentního párového programování:

### 🛠️ AI Vývojová platforma
- **Google DeepMind Antigravity** — pokročilé agentní vývojové prostředí s přímým přístupem k NixOS terminálu, kompilaci PlatformIO, ladění I/O a USB flashování desky (`/dev/ttyACM0`).

### 🧠 Použité AI modely během vývoje
1. **Gemini 3.8 Flash (High)** (`gemini-3.8-flash-high` / `gemini-3.8-flash-n`):
   - Architektura projektu, konfigurace `platformio.ini` a `shell.nix` pro NixOS.
   - Rešerše pinoutu čipu ESP32-PICO-D4 a vyřešení fatálního pádu Watchdogu (`TG1WDT_SYS_RESET`) způsobeného kolizí interní SPI flash paměti na GPIO 16 s resetovacím pinem OLED.
   - Nízkoúrovňová komunikace s transceiverem SX1278 přes přímé SPI registry (vynechání privátních metod knihovny `LoRa`).
   - Implementace původního Doom ohně s 8×8 Bayerovým ditheringem.
   - Odhalení a oprava aritmetického podtečení náhodného jitteru (`uint8_t`), které způsobovalo divoké blikání displeje.
   - Návrh a implementace režimů **⚡ Tesla blesky** a **📡 Cyberpunk analyzátor**.
   - Vyřešení absence fyzického tlačítka BOOT (implementace přepínání přes softwarovou detekci stisku tlačítka RST `ESP_RST_EXT`, kapacitní dotyk na GPIO 4 a USB sériové příkazy).
   - Trvalé vypnutí rušivě blikající zelené indikační LED na GPIO 25.
2. **Claude Opus 4.6 (Thinking)** (`claude-opus-4-6-thinking`):
   - Matematická rekalibrace výšky plamene ohně tak, aby odpovídala reálné úrovni elektromagnetického šumu v místnosti.
   - Návrh a odladění filtru exponenciálního klouzavého průměru (EMA) pro vyhlazení okamžitého RSSI a wideband registru.
   - Zvýšení plynulosti a ladění telemetrického reportování.
3. **Claude Sonnet 4.6 (Thinking)** (`claude-sonnet-4-6-thinking`):
   - Návrh a implementace 4. režimu: **🌌 3D Starfield Warp Speed let** s plnou 3D perspektivní projekcí 72 hvězd.
   - Algoritmus pro znovuzrozování hvězd v hloubce vesmíru s novou RF entropií a natahování do světelných laserových paprsků (*warp streaks*).
   - Ladění a postupné zpomalení letu na uklidňující, majestátní kosmický drift.

👉 **Úplný záznam průběhu vývoje, ladicích kroků a promptů si můžete přečíst v souboru [CONVERSATION.md](CONVERSATION.md).**

---

## 🎮 5 Režimů vizualizace

### 1. 🌌 VISUAL_STARFIELD_WARP (3D Průlet hvězdami / Warp Speed Flight)
- **Vizuál:** 72 trojrozměrných hvězd s plnou 3D perspektivní projekcí proudících z hloubi vesmíru přímo proti vám.
- **Warp efekt:** 
  - Při klidném éteru majestátní a pomalý průlet hlubokým vesmírem rychlostí ~0.10c–0.20c (hvězdy klidně a plynule proplouvají prostorem).
  - Při vyšším rušení rychlost citlivě akceleruje k 0.5c–0.7c a hvězdy se natahují do světelných laserových paprsků (Warp streaks).
  - Při silném RF impulsu / detekci paketu proběhne **krátký svižný skok do hyperprostoru** (~1.8c).
- **Řízení kurzu:** Kosmický vítr a frekvenční gradient naklánějí úběžník letu (Yaw/Pitch).
- **HUD:** `WARP 433.92M -74dB 0.1c` s jemným středovým zaměřovačem.

### 2. ⚡ VISUAL_TESLA_LIGHTNING (Elektrická plazma / Blesková bouře)
- **Vizuál:** Ze spodní cívky/elektrody šlehají do tmy procedurální rozvětvené blesky a létají z nich plazmové částice.
- **Reakce na rušení:** Při klidu v místnosti jde o jemné jiskření kolem elektrody. Při zvýšení rušení vyšlehnou až 4–6 masivních rozvětvených blesků přes celou obrazovku.

### 3. 📡 VISUAL_CYBER_SPECTRUM (Cyberpunk RF Spektrální analyzátor & Vektorový skop)
- **Horní část (y=0..20):** 9-sloupcový spektrální analyzátor pro 9 kanálů v pásmu 433 MHz s gravitačně klesajícími **Peak Hold** indikátory.
- **Střední část (y=22..53):** Vektorový analogový osciloskop s retikulem a živou spojitou křivkou deformovanou rádiovým a elektrostatickým šumem.
- **Spodní část (y=55..63):** Sci-fi telemetrická páska (`433.92M -74dB [0x0E 0512 PKT:0]`) se surovými hex byty entropie.

### 4. 🔥 VISUAL_COSMIC_FIRE (Původní Doom Fire)
- Termodynamický oheň s 8×8 Bayerovým ditheringem a adaptivní výškou plamenů.

### 5. 🎲 VISUAL_TRUE_RANDOM_NUM (105-místný Hardware TRNG výhradně z antény)
- **Vizuál:** Masivní 105-ciferné náhodné číslo rozprostřené přes celou obrazovku v 5 řádcích po 21 číslicích (font 6×10).
- **Perioda:** Nové náhodné číslo se vygeneruje přesně jednou za 10 sekund (10s interval).
- **100% Čistý šum z antény (absolutně žádné jiné zdroje):**
  - Při generování nehraje roli **vůbec nic jiného než elektromagnetický a kosmický šum přicházející z antény**:
    1. **SX1278 Wideband RSSI (reg 0x2C):** Širokopásmový detektor energie přijímané z antény v pásmu 433 MHz.
    2. **Semtech AN1200.24 Von Neumann debiasing:** Z anténního registru 0x2C se odebírají dvojice LSB bitů. Dvojice `01` generuje bit 0, `10` generuje bit 1, shodné dvojice `00` a `11` se zahazují. Tím je matematicky garantována striktní pravděpodobnost $P(0) = P(1) = 0.5$ bez jakéhokoliv posunu.
    3. **Kompletní eliminace ne-anténních zdrojů:** Z kódu byly zcela odstraněny interní ESP32 TRNG (`esp_random`), plovoucí ADC piny i čítače CPU cyklů.
- **Rejection Sampling (nulové zkreslení):**
  - Z debiasovaných bajtů z antény se hodnoty 0..249 mapují rovnoměrně na číslice 0..9 (každá má identickou pravděpodobnost přesně 10,00 %). Hodnoty 250..255 se zahazují a odebírá se nový vzorek z antény, což zcela eliminuje modulo zkreslení (*modulo bias*).
- **HUD & Odpočet:**
  - Horní stavový řádek zobrazuje číslo generovaného bloku a zbývající sekundy (`TRNG #4 433MHz  [ 7s ]`).
  - Spodní plynulý indikátor (progress bar na y=63) graficky odpočítává 10 sekund do dalšího čísla.

---

## 🕹️ Přepínání režimů

1. **Tlačítkem RST na boku desky:** Jedním stiskem tlačítka RST se deska restartuje a ihned přeskočí na následující režim (uloží se i do paměti flash).
2. **Automatický kolotoč (Auto-Cycle):** Režimy se samy plynule střídají každých 20 sekund jako živá galerie.
3. **Kapacitním dotykem:** Dotkněte se prstem pinu **GPIO 4** na liště desky.
4. **Sériovým monitorem přes USB:** Klávesy `1`, `2`, `3`, `4`, `5`, `Mezerník` nebo `A` (vypnutí/zapnutí kolotoče).

---

## 🌐 Volitelné Wi-Fi připojení & Webový Server (HTTP API)

Deska nabízí volitelnou možnost připojení k lokální síti a spuštění HTTP serveru pro generování libovolně dlouhých náhodných čísel přímo z antény přes síť.

### ⚙️ Konfigurace v souboru `include/wifi_config.h`
Veškeré síťové nastavení je bezpečně odděleno v souboru [include/wifi_config.h](include/wifi_config.h):

```c
// 1. Povolení / zakázání Wi-Fi (true = zapnuto, false = zcela vypnuto bez vlivu na systém)
#define WIFI_ENABLED            true

// 2. Přihlašovací údaje k Wi-Fi
#define WIFI_SSID               "Vase-WiFi-SSID"
#define WIFI_PASSWORD           "Vase-WiFi-Heslo"

// 3. Volba DHCP vs. Statická IP adresa
#define USE_STATIC_IP           false   // false = DHCP, true = statická IP níže

// 4. Nastavení statické IP (použije se při USE_STATIC_IP == true)
#define STATIC_IP               192, 168, 1, 50
#define STATIC_GATEWAY          192, 168, 1, 1
#define STATIC_SUBNET           255, 255, 255, 0
```

### 🚀 Použití Webového API
Po připojení k síti běží na desce lehký webový server:

1. **Získání náhodného čísla zadané délky (Plain Text):**
   ```bash
   # Získání 32-místného náhodného čísla:
   curl http://192.168.1.50/32

   # Získání 100-místného náhodného čísla:
   curl http://192.168.1.50/100
   ```
2. **Alternativní formát URL:**
   ```bash
   curl http://192.168.1.50/cislo?len=64
   ```
3. **JSON výstup:**
   ```bash
   curl http://192.168.1.50/64?format=json
   ```
4. **Interaktivní webové rozhraní:**
   Při otevření adresy `http://192.168.1.50/` v běžném webovém prohlížeči (na mobilu či PC) se zobrazí přehledné tmavé rozhraní s volbou počtu cifer, tlačítkem pro okamžité vygenerování a tlačítkem pro zkopírování čísla do schránky.

> [!NOTE]
> **Nulový dopad na běh systému:** Pokud je Wi-Fi vypnuta nebo se zrovna nepoužívá, připojování a obsluha probíhá neblokujícím způsobem na pozadí a nemá absolutně žádný vliv na plynulost animací ani vykreslování OLED displeje (45–50 FPS). Každá číslice je generována přímo z fyzického RF šumu čipu SX1278.
