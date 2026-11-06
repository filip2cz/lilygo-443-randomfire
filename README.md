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

### 5. 🎲 VISUAL_TRUE_RANDOM_NUM (105-místný Hardware TRNG z fyzického šumu)
- **Vizuál:** Masivní 105-ciferné náhodné číslo rozprostřené přes celou obrazovku v 5 řádcích po 21 číslicích (font 6×10).
- **Perioda:** Nové náhodné číslo se vygeneruje přesně jednou za 10 sekund (10s interval).
- **100% Fyzická entropie (žádná pseudonáhoda / PRNG):**
  - Každá číslice '0'..'9' je vzorkována v reálném čase přímo ze surových fyzických šumů:
    1. **SX1278 Wideband RSSI (reg 0x2C):** Širokopásmový detektor energie v pásmu 433 MHz.
    2. **SX1278 Channel RSSI (reg 0x1B):** Úroveň okamžitého rádiového signálu a šum pozadí.
    3. **Plovoucí pin ADC (GPIO 34):** Tepelný elektrostatický šum na nezapojeném analogovém vstupu.
    4. **ESP32 interní hardware TRNG (`esp_random()`):** Vyhrazená hardwarová periferie generující entropii ze šumu v RF subsystému (Wi-Fi/BT ADC).
    5. **CPU Clock Jitter (`esp_cpu_get_ccount()`):** Mikroskopický časový fázový šum krystalového oscilátoru.
  - Všechny fyzické zdroje jsou smíchány čistě bitovým XORem bez jakéhokoliv matematického PRNG algoritmu nebo seedu.
- **Rejection Sampling (nulové zkreslení):**
  - Hodnoty 0..249 se mapují rovnoměrně na číslice 0..9 (každá má identickou pravděpodobnost přesně 10,00 %). Hodnoty 250..255 se zahazují a odebírá se nový vzorek fyzického šumu, což zcela eliminuje modulo zkreslení (*modulo bias*).
- **HUD & Odpočet:**
  - Horní stavový řádek zobrazuje číslo generovaného bloku a zbývající sekundy (`TRNG #4 433MHz  [ 7s ]`).
  - Spodní plynulý indikátor (progress bar na y=63) graficky odpočítává 10 sekund do dalšího čísla.

---

## 🕹️ Přepínání režimů

1. **Tlačítkem RST na boku desky:** Jedním stiskem tlačítka RST se deska restartuje a ihned přeskočí na následující režim (uloží se i do paměti flash).
2. **Automatický kolotoč (Auto-Cycle):** Režimy se samy plynule střídají každých 20 sekund jako živá galerie.
3. **Kapacitním dotykem:** Dotkněte se prstem pinu **GPIO 4** na liště desky.
4. **Sériovým monitorem přes USB:** Klávesy `1`, `2`, `3`, `4`, `5`, `Mezerník` nebo `A` (vypnutí/zapnutí kolotoče).
