# Cosmic Random Fire — LilyGo LoRa32 T3_V1.6.1 (433 MHz)

Program pro desku **LilyGo LoRa32 T3_V1.6.1 (433MHz)**, který generuje **zcela náhodný oheň a plazmu** na integrovaném OLED displeji na základě **skutečného fyzikálního šumu a elektromagnetického rušení z okolního prostoru**.

---

## 🌌 Zdroje fyzikálního šumu a entropie

Program nepoužívá žádný pseudonáhodný generátor se statickým semínkem. Každý pixel, plamen i jiskra jsou přímo řízeny živou fyzikální entropií:

1. **SX1278 Wideband RF Front-End (433 MHz)**:
   - Rádiový čip Semtech SX1278 je přepnut do širokopásmového režimu (500 kHz šířka pásma) s maximálním zesílením LNA (+20 dB).
   - Průběžně vzorkuje registr širokopásmového šumu `0x2C` (`RegRssiWideband`), který reaguje na tepelný šum antény (Johnson-Nyquistův šum), kosmické mikrovlnné pozadí, atmosférické výboje (blesky/sferiky) a okolní rádiové rušení.
   - Snímá kanálové RSSI v reálném čase.
2. **Frekvenční skenování (Frequency Hopping)**:
   - Každých 180 ms automaticky přepíná mezi frekvencemi v pásmu 433 MHz (433.05, 433.175, 433.375, 433.575, 433.92, 434.175, 434.65 MHz).
   - Rozdíl v rušení mezi pásmy určuje vektor **kosmického větru** – plameny se fyzicky naklánějí podle směru a charakteru rušení.
3. **Detekce paketů a RF záblesků ("Sluneční erupce")**:
   - Pokud v okolí někdo vyšle signál (klíčenka od auta na 433.92 MHz, meteorologická stanice, bezdrátový zvonek nebo LoRa paket), detekce vyvolá masivní plazmovou erupci a vymrštění desítek jisker vzhůru.
4. **Plovoucí ADC pin (Elektrostatická a částicová anténa)**:
   - Nevyužitý vysokoimpedanční pin GPIO 34 funguje jako miniaturní anténa zachycující elektrostatické pole v místnosti, 50Hz síťový brum a ionizaci vzduchu.
5. **Hardware TRNG ESP32**:
   - Vnitřní hardwarový generátor náhodných čísel ESP32 (`esp_random()`), vzorkující termální šum v analogových RF obvodech Wi-Fi/BT.
6. **Časový a cyklový jitter**:
   - Registr CPU cyklů (`esp_cpu_get_ccount()`) zachycující mikrosekundový jitter oscilátoru.

Všechny tyto toky jsou v reálném čase míchány v chaotickém mixéru a každou sekundu dodávají miliony bytů čerstvé fyzikální entropie.

---

## 🔥 Algoritmus simulace ohně a dithering

- **Termodynamické pole (128 × 64)**: Každý bod má hodnotu tepla 0–255.
- **Konvekce a difuze**: Teplo stoupá vzhůru, chladne v závislosti na okolním šumu a je unášeno stochastickým větrem.
- **8×8 Bayer Dithering**: Pro zobrazení 256 úrovní tepla na 1-bitovém monochromatickém OLED displeji SSD1306 se používá uspořádaný Bayerův dithering obohacený o šumový jitter. Výsledkem jsou hladké polotóny a přirozený retro plazmový vzhled.
- **Částicový systém jisker (Embers)**: Silné šumové impulzy vystřelují z ohně zářící jiskry, které stoupají k hornímu okraji displeje a postupně dohořívají.
- **Hardwarové LED světlo ohně**: Vestavěná modrá LED (GPIO 25) pomocí hardwarového PWM kanálu mihotavě bliká a vrhá do místnosti světlo ohně synchronizované se simulací na displeji.

---

## 🕹️ Přepínání režimů zobrazení (Tlačítko BOOT / GPIO 0)

Stisknutím tlačítka **BOOT** (GPIO 0) na desce můžete plynule přepínat mezi 4 režimy:

1. **MODE 0: Oheň + Telemetrický HUD**
   - Plnohodnotný oheň s horní stavovou lištou zobrazující aktuální frekvenci (např. `433.92M`), RSSI v dBm (`-73dB`), wideband šum a pulzující indikátor toku entropie.
2. **MODE 1: Čistý inferno (Fullscreen)**
   - 100% plocha displeje (všech 64 řádků) věnována čistému plápolajícímu ohni bez jakýchkoliv textů či rušivých prvků.
3. **MODE 2: RF Osciloskop / Spektrum + Oheň**
   - Horní polovina: Živý osciloskop rádiového šumu a rušení v reálném čase.
   - Spodní polovina: Oheň živený přímo zobrazeným šumem.
4. **MODE 3: Statistika kosmické entropie**
   - Podrobný přehled: Frekvence, RSSI, Wideband registr, hodnota ADC elektrostatického šumu a počet zachycených paketů.

---

## ⚙️ Vývoj a nahrávání v NixOS

Projekt je připraven pro NixOS s přiloženým `shell.nix`.

### 1. Spuštění prostředí
```bash
nix-shell
```

### 2. Kompilace projektu
```bash
pio run
```

### 3. Nahrání firmwaru do desky (LilyGo LoRa32)
Deska je připojena na `/dev/ttyACM1` (nebo `/dev/ttyUSB0` dle systému):
```bash
pio run -t upload
```

### 4. Sledování sériového monitoru (FPS a telemetrie)
```bash
pio device monitor
```
*(Běží na rychlosti 115200 baudů, snímková frekvence je stabilních ~42 FPS).*

---

## 📌 Zapojení pinů (LilyGo T3 V1.6.1)

| Periferie | Pin ESP32 | Funkce |
|---|---|---|
| **OLED SDA** | GPIO 21 | I2C Data (800 kHz) |
| **OLED SCL** | GPIO 22 | I2C Clock (800 kHz) |
| **LoRa SCK** | GPIO 5 | SPI Clock |
| **LoRa MISO** | GPIO 19 | SPI MISO |
| **LoRa MOSI** | GPIO 27 | SPI MOSI |
| **LoRa CS (NSS)** | GPIO 18 | SPI Chip Select |
| **LoRa RST** | GPIO 23 | Reset transceivery |
| **LoRa DIO0** | GPIO 26 | Přerušení paketu |
| **Tlačítko BOOT** | GPIO 0 | Přepínání režimů displeje |
| **Stavová LED** | GPIO 25 | Hardwarový PWM mihotavý svit |
| **Šumový ADC** | GPIO 34 | Plovoucí elektrostatická anténa |
