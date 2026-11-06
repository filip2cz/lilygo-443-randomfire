# Cosmic Random Interference Visualizer — LilyGo LoRa32 T3_V1.6.1 (433 MHz)

Program pro desku **LilyGo LoRa32 T3_V1.6.1 (433MHz)**, který snímá náhodné rušení z okolního elektromagnetického prostředí, kosmický šum a signály v pásmu 433 MHz pomocí transceivery SX1278 a ESP32 hardware TRNG/ADC.

Na OLED displeji SSD1306 (128×64) nabízí **3 přepínatelné vizualizační režimy**.

---

## 🎮 3 Režimy vizualizace

### 1. ⚡ VISUAL_TESLA_LIGHTNING (Elektrická plazma / Blesková bouře)
- **Vizuál:** Ze spodní Tesla cívky / elektrody šlehají do prostoru procedurální rozvětvené elektrické výboje s bleskovou geometrií a plazmovými částicemi.
- **Reakce na rušení:** 
  - Při klidu cracklí jen jemné jiskření kolem elektrody.
  - Při silném rušení nebo detekci signálu (např. klíče od auta) šlehne až 6 masivních rozvětvených blesků přes celou obrazovku.
- **LED:** Vestavěná LED (GPIO 25) pulzuje a bleskově problikává jako stroboskop při každém výboji.

### 2. 📡 VISUAL_CYBER_SPECTRUM (Cyberpunk RF spektrální analyzátor & vektorový skop)
- **Horní část (y=0..20):** 9-sloupcový spektrální analyzátor pro 9 kanálů v pásmu 433 MHz (433.05 až 434.65 MHz) s **Peak Hold** indikátory, které líně klesají s gravitací.
- **Střední část (y=22..53):** Vektorový osciloskop s retikulem a spojitou analogovou křivkou reagující na surový elektrostatický a rádiový šum.
- **Spodní část (y=55..63):** Sci-fi telemetrická páska s aktuální frekvencí, RSSI a surovými hex byty entropie.
- **LED:** Intenzita svitu přímo kopíruje energii signálu na aktivní frekvenci.

### 3. 🔥 VISUAL_COSMIC_FIRE (Kosmický oheň - Doom Fire Engine)
- Klasický Doom Fire algoritmus s uspořádaným 8×8 Bayerovým ditheringem.
- Výška a energie plamene se adaptivně přizpůsobuje množství rušení v místnosti.
- **LED:** Mihotá se jako světlo z krbu.

---

## 🕹️ Přepínání režimů

1. **Hardwarově za běhu:** Stiskněte tlačítko **BOOT (GPIO 0)** na desce. Na 1,4 s se zobrazí stylový rámeček s názvem nově aktivovaného režimu.
2. **V kódu jako výchozí režim:** V souboru [src/main.cpp](file:///home/filip/git/lilygo-443-randomfire/src/main.cpp) hned nahoře:
```cpp
// 0 = VISUAL_TESLA_LIGHTNING, 1 = VISUAL_CYBER_SPECTRUM, 2 = VISUAL_COSMIC_FIRE
VisualMode activeVisualMode = VISUAL_TESLA_LIGHTNING;
```

---

## ⚙️ Vývoj a nahrávání v NixOS

```bash
# Vstup do prostředí NixOS
nix-shell

# Nahrání do desky (automatická detekce portu)
pio run -t upload

# Sledování FPS a telemetrie (stabilních ~45 FPS)
pio device monitor
```
