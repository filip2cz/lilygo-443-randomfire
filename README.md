# Cosmic Random Interference Visualizer — LilyGo LoRa32 T3_V1.6.1 (433 MHz)

Program pro desku **LilyGo LoRa32 T3_V1.6.1 (433MHz)**, který snímá náhodné rušení z okolního elektromagnetického prostředí, kosmický šum a rádiové signály v pásmu 433 MHz pomocí transceiveru SX1278 a ESP32 hardware TRNG/ADC.

Na OLED displeji SSD1306 (128×64) nabízí **4 přepínatelné vizualizační režimy**.

---

## 🎮 4 Režimy vizualizace

### 1. 🌌 VISUAL_STARFIELD_WARP (3D Průlet hvězdami / Warp Speed Flight)
- **Vizuál:** 72 trojrozměrných hvězd s plnou 3D perspektivní projekcí proudících z hloubi vesmíru přímo proti vám.
- **Warp efekt:** 
  - Při klidném éteru majestátní a pomalý průlet hlubokým vesmírem rychlostí ~0.4c–0.6c (blízké hvězdy jemně září a plynule proplouvají).
  - Při vyšším rušení rychlost citlivě akceleruje k 1.2c–1.5c a hvězdy se natahují do světelných laserových paprsků (Warp streaks).
  - Při silném RF impulsu / detekci paketu proběhne **krátký záblesk skoku do hyperprostoru** (~3.8c).
- **Řízení kurzu:** Kosmický vítr a frekvenční gradient naklánějí úběžník letu (Yaw/Pitch).
- **HUD:** `WARP 433.92M -74dB 0.5c` s jemným středovým zaměřovačem.

### 2. ⚡ VISUAL_TESLA_LIGHTNING (Elektrická plazma / Blesková bouře)
- **Vizuál:** Ze spodní cívky/elektrody šlehají do tmy procedurální rozvětvené blesky a létají z nich plazmové částice.
- **Reakce na rušení:** Při klidu v místnosti jde o jemné jiskření kolem elektrody. Při zvýšení rušení vyšlehnou až 4–6 masivních rozvětvených blesků přes celou obrazovku.

### 3. 📡 VISUAL_CYBER_SPECTRUM (Cyberpunk RF Spektrální analyzátor & Vektorový skop)
- **Horní část (y=0..20):** 9-sloupcový spektrální analyzátor pro 9 kanálů v pásmu 433 MHz s gravitačně klesajícími **Peak Hold** indikátory.
- **Střední část (y=22..53):** Vektorový analogový osciloskop s retikulem a živou spojitou křivkou deformovanou rádiovým a elektrostatickým šumem.
- **Spodní část (y=55..63):** Sci-fi telemetrická páska (`433.92M -74dB [0x0E 0512 PKT:0]`) se surovými hex byty entropie.

### 4. 🔥 VISUAL_COSMIC_FIRE (Původní Doom Fire)
- Termodynamický oheň s 8×8 Bayerovým ditheringem a adaptivní výškou plamenů.

---

## 🕹️ Přepínání režimů

1. **Tlačítkem RST na boku desky:** Jedním stiskem tlačítka RST se deska restartuje a ihned přeskočí na následující režim (uloží se i do paměti flash).
2. **Automatický kolotoč (Auto-Cycle):** Režimy se samy plynule střídají každých 20 sekund jako živá galerie.
3. **Kapacitním dotykem:** Dotkněte se prstem pinu **GPIO 4** na liště desky.
4. **Sériovým monitorem přes USB:** Klávesy `1`, `2`, `3`, `4`, `Mezerník` nebo `A` (vypnutí/zapnutí kolotoče).
