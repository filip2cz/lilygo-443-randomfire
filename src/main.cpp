/*******************************************************************************
 * COSMIC RANDOM FIRE - LilyGo T3 LoRa32 V1.6.1 (433MHz)
 * 
 * Snímá náhodné rušení z okolního elektromagnetického prostředí, kosmický šum,
 * atmosférické výboje a fluktuace na frekvenci 433 MHz pomocí transceiveru SX1278
 * a ESP32 hardware TRNG/ADC.
 * Na OLED displeji (SSD1306) v reálném čase vykresluje autentický, stochasticky
 * řízený plamen (Doom Fire engine) ditherovaný 8x8 Bayerovou maticí.
 *******************************************************************************/

#include <Arduino.h>
#include <SPI.h>
#include <LoRa.h>
#include <Wire.h>
#include <U8g2lib.h>

// =============================================================================
// HARDWARE PIN DEFINITIONS (LilyGo LoRa32 T3 V1.6.1)
// =============================================================================
#define OLED_SDA        21
#define OLED_SCL        22
#undef OLED_RST
#define OLED_RST        U8X8_PIN_NONE // GPIO 16 is NOT used (internal SPI flash on ESP32-PICO-D4)

#define LORA_SCK        5
#define LORA_MISO       19
#define LORA_MOSI       27
#define LORA_CS         18
#define LORA_RST        23
#define LORA_IRQ        26

#define BOARD_LED       25
#define BOOT_BUTTON     0
#define ADC_NOISE_PIN   34 // Unconnected floating high-impedance ADC1 pin

// Display dimensions
#define SCREEN_WIDTH    128
#define SCREEN_HEIGHT   64

// Display instance
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, OLED_RST, OLED_SCL, OLED_SDA);

// =============================================================================
// 8x8 BAYER DITHERING MATRIX (0..63 mapped to 0..252)
// =============================================================================
static const uint8_t PROGMEM BAYER_8X8[8][8] = {
    {  0, 32,  8, 40,  2, 34, 10, 42 },
    { 48, 16, 56, 24, 50, 18, 58, 26 },
    { 12, 44,  4, 36, 14, 46,  6, 38 },
    { 60, 28, 52, 20, 62, 30, 54, 22 },
    {  3, 35, 11, 43,  1, 33,  9, 41 },
    { 51, 19, 59, 27, 49, 17, 57, 25 },
    { 15, 47,  7, 39, 13, 45,  5, 37 },
    { 63, 31, 55, 23, 61, 29, 53, 21 }
};

// =============================================================================
// FREQUENCY SWEEP TABLE (433 MHz ISM Band)
// =============================================================================
static const long FREQ_CHANNELS[] = {
    433050000L, // Spodní okraj pásma
    433175000L, // LoRa kanál 1
    433375000L, // LoRa kanál 2
    433575000L, // LoRa kanál 3
    433775000L, // LoRa kanál 4
    433920000L, // Střed pásma (klíče aut, meteostanice, čidla - nejvíce rušení)
    434175000L, // Vyšší LoRa
    434420000L, // Telemetrie
    434650000L  // Horní okraj pásma
};
static const uint8_t NUM_CHANNELS = sizeof(FREQ_CHANNELS) / sizeof(FREQ_CHANNELS[0]);

// =============================================================================
// SPARK PARTICLE SYSTEM (Ejected embers from intense noise events)
// =============================================================================
#define MAX_SPARKS 24
struct Spark {
    float x;
    float y;
    float vx;
    float vy;
    uint8_t life;
    bool active;
};

// =============================================================================
// GLOBAL STATE & TELEMETRY
// =============================================================================
enum DisplayMode {
    MODE_FIRE_HUD = 0,     // Oheň s elegantní telemetrickou lištou
    MODE_FIRE_ONLY = 1,    // 100% fullscreen čistý oheň (bez textu)
    MODE_RF_WATERFALL = 2, // Živý osciloskop rádiového šumu + oheň
    MODE_COSMIC_STATS = 3, // Statistiky kosmické entropie
    MODE_COUNT = 4
};

DisplayMode currentMode = MODE_FIRE_HUD;

// Telemetry
int currentRssi = -115;
uint8_t currentWideband = 0;
uint8_t currentChannelIdx = 5; // 433.92 MHz
uint32_t entropyCount = 0;
uint32_t packetCount = 0;
uint16_t adcNoiseVal = 0;
int cosmicWind = 0;             // -1 (doleva), 0, +1 (doprava)
uint8_t burstFramesLeft = 0;    // Počet snímků plazmové erupce
float fps = 0.0f;

// History buffer for waterfall oscilloscope (128 columns)
uint8_t noiseHistory[SCREEN_WIDTH];

// 2D Heat Buffer (128x64)
uint8_t heat[SCREEN_HEIGHT][SCREEN_WIDTH];
Spark sparks[MAX_SPARKS];

// 64-bit Chaotic Entropy State stirred by physical noise
uint64_t entropyState = 0x1337BEEFDEADBEEFULL;

// Direct SX1278 SPI register access
uint8_t readSX1278Register(uint8_t reg) {
    digitalWrite(LORA_CS, LOW);
    SPI.transfer(reg & 0x7F);
    uint8_t val = SPI.transfer(0x00);
    digitalWrite(LORA_CS, HIGH);
    return val;
}

void writeSX1278Register(uint8_t reg, uint8_t val) {
    digitalWrite(LORA_CS, LOW);
    SPI.transfer((reg & 0x7F) | 0x80);
    SPI.transfer(val);
    digitalWrite(LORA_CS, HIGH);
}

// Fast inline chaotic generator (1 CPU cycle per byte)
static inline uint8_t harvestEntropyByte() {
    entropyCount++;
    entropyState = entropyState * 6364136223846793005ULL + 1442695040888963407ULL;
    return (uint8_t)(entropyState >> 56);
}

// =============================================================================
// SPARK EMBER SYSTEM
// =============================================================================
void initSparks() {
    for (int i = 0; i < MAX_SPARKS; i++) {
        sparks[i].active = false;
    }
}

void spawnSpark(int x, float power) {
    for (int i = 0; i < MAX_SPARKS; i++) {
        if (!sparks[i].active) {
            sparks[i].active = true;
            sparks[i].x = (float)x;
            sparks[i].y = (float)(SCREEN_HEIGHT - 3);
            sparks[i].vx = ((float)(harvestEntropyByte() % 100) - 50.0f) / 50.0f + ((float)cosmicWind * 0.3f);
            sparks[i].vy = -((float)(harvestEntropyByte() % 100) / 40.0f + 1.2f + power * 1.5f);
            sparks[i].life = (uint8_t)(harvestEntropyByte() % 20 + 20);
            break;
        }
    }
}

void updateSparks() {
    for (int i = 0; i < MAX_SPARKS; i++) {
        if (sparks[i].active) {
            sparks[i].x += sparks[i].vx;
            sparks[i].y += sparks[i].vy;
            sparks[i].vy += 0.03f; // Gravitační zbrzdění
            sparks[i].vx += ((float)cosmicWind * 0.04f);
            
            if (sparks[i].life > 0) {
                sparks[i].life--;
            } else {
                sparks[i].active = false;
            }
            
            if (sparks[i].x < 0 || sparks[i].x >= SCREEN_WIDTH || sparks[i].y < 0 || sparks[i].y >= SCREEN_HEIGHT) {
                sparks[i].active = false;
            }
        }
    }
}

// =============================================================================
// AUTHENTIC DOOM FIRE SIMULATION ENGINE
// =============================================================================
void initFire() {
    memset(heat, 0, sizeof(heat));
    memset(noiseHistory, 0, sizeof(noiseHistory));
    initSparks();
}

void updateFirePhysics() {
    int bottomRow = SCREEN_HEIGHT - 1;
    
    // Vliv okolního rádiového rušení na intenzitu paliva
    // Klidný pokoj (-120 dBm) -> rfEnergy = 0; Rušení (-70 dBm) -> rfEnergy = 50
    int rfEnergy = map(constrain(currentRssi, -125, -60), -125, -60, 0, 50);
    if (burstFramesLeft > 0) {
        rfEnergy = 80;
        burstFramesLeft--;
    }
    
    // 1. Spodní řádek: Generování žhavých uhlíků (Fuel Line)
    for (int x = 0; x < SCREEN_WIDTH; x++) {
        uint8_t r = harvestEntropyByte();
        
        // Většina pixelů tvoří žhavé uhlíky (230-255)
        // Občasné mezery ((r & 0x07) == 0) vytvářejí přirozené jazyky plamene
        if ((r & 0x07) == 0) {
            heat[bottomRow][x] = 0; // Mezera mezi plameny
        } else {
            int fuel = 200 + rfEnergy + (r & 0x1F);
            heat[bottomRow][x] = (uint8_t)constrain(fuel, 0, 255);
            
            // Špička rušení vymrští jiskru
            if (r > 248 || (burstFramesLeft > 0 && r > 230)) {
                spawnSpark(x, (float)rfEnergy / 50.0f);
            }
        }
    }
    
    // 2. Doom Fire propagace tepla směrem vzhůru s chladnutím a driftem
    for (int y = 1; y < SCREEN_HEIGHT; y++) {
        for (int x = 0; x < SCREEN_WIDTH; x++) {
            uint8_t pixel = heat[y][x];
            
            if (pixel == 0) {
                // Žádné teplo nepřenášet
                if (heat[y - 1][x] > 4) heat[y - 1][x] -= 4;
                else heat[y - 1][x] = 0;
            } else {
                uint8_t r = harvestEntropyByte();
                
                // Chladnutí: základ 3 až 6, při vyšším rušení plameny šlehají výše
                int decay = 3 + (r & 3);
                if (rfEnergy > 30 && decay > 3) decay--; // Nižší chladnutí = vyšší plamen
                if (burstFramesLeft > 0) decay = 1 + (r & 1); // Solární erupce šlehá k nebi
                
                // Drift: -1, 0, +1 ovlivněný kosmickým větrem
                int drift = ((int)(r >> 2) & 3) - 1 + cosmicWind;
                
                int dstX = x + drift;
                if (dstX < 0) dstX = 0;
                else if (dstX >= SCREEN_WIDTH) dstX = SCREEN_WIDTH - 1;
                
                int dstY = y - 1;
                
                int cooled = (int)pixel - decay;
                heat[dstY][dstX] = (cooled > 0) ? (uint8_t)cooled : 0;
            }
        }
    }
    
    // 3. Aktualizace jisker
    updateSparks();
}

// =============================================================================
// DITHERED FRAMEBUFFER RENDERING (SSD1306)
// =============================================================================
void renderFireToBuffer() {
    uint8_t* pBuf = u8g2.getBufferPtr();
    
    // 8 Paged horizontal stripes (each page = 8 vertical pixels)
    for (int page = 0; page < 8; page++) {
        int yBase = page * 8;
        for (int x = 0; x < SCREEN_WIDTH; x++) {
            uint8_t byteVal = 0;
            for (int bit = 0; bit < 8; bit++) {
                int y = yBase + bit;
                uint8_t h = heat[y][x];
                
                // 8x8 Bayer dithering threshold: striktně 0..252 (žádný podtečení!)
                uint16_t threshold = ((uint16_t)pgm_read_byte(&BAYER_8X8[y & 7][x & 7])) * 4;
                if (h > threshold) {
                    byteVal |= (1 << bit);
                }
            }
            pBuf[page * SCREEN_WIDTH + x] = byteVal;
        }
    }
    
    // Vykreslení zářících jisker na černou oblohu nad plameny
    for (int i = 0; i < MAX_SPARKS; i++) {
        if (sparks[i].active) {
            int sx = (int)sparks[i].x;
            int sy = (int)sparks[i].y;
            if (sx >= 0 && sx < SCREEN_WIDTH && sy >= 0 && sy < SCREEN_HEIGHT) {
                pBuf[(sy / 8) * SCREEN_WIDTH + sx] |= (1 << (sy % 8));
                // 2-pixelový vertikální svit pro výrazné jiskry
                if (sy > 0) {
                    pBuf[((sy - 1) / 8) * SCREEN_WIDTH + sx] |= (1 << ((sy - 1) % 8));
                }
            }
        }
    }
}

// =============================================================================
// SCREEN OVERLAYS & MODES
// =============================================================================
void renderWaterfallMode() {
    uint8_t* pBuf = u8g2.getBufferPtr();
    
    // Vyčistit horních 24 řádků pro RF osciloskop
    for (int page = 0; page < 3; page++) {
        for (int x = 0; x < SCREEN_WIDTH; x++) {
            pBuf[page * SCREEN_WIDTH + x] = 0;
        }
    }
    
    // Vykreslit křivku rádiového šumu
    for (int x = 0; x < SCREEN_WIDTH; x++) {
        int y = constrain(map(noiseHistory[x], 0, 48, 22, 2), 2, 22);
        pBuf[(y / 8) * SCREEN_WIDTH + x] |= (1 << (y % 8));
        // Středová mřížka
        if (x % 6 == 0) {
            pBuf[(12 / 8) * SCREEN_WIDTH + x] |= (1 << (12 % 8));
        }
    }
    
    // Oddělovací linka
    for (int x = 0; x < SCREEN_WIDTH; x++) {
        pBuf[(23 / 8) * SCREEN_WIDTH + x] |= (1 << (23 % 8));
    }
}

void drawHUD() {
    u8g2.setFontMode(1);
    
    if (currentMode == MODE_FIRE_HUD) {
        // Elegantní horní lišta s černým pozadím
        u8g2.setDrawColor(0);
        u8g2.drawBox(0, 0, 128, 9);
        u8g2.setDrawColor(1);
        u8g2.drawHLine(0, 9, 128);
        
        u8g2.setFont(u8g2_font_5x7_tf);
        
        // Frekvence
        char str[32];
        float mhz = (float)FREQ_CHANNELS[currentChannelIdx] / 1000000.0f;
        snprintf(str, sizeof(str), "%.2fM", mhz);
        u8g2.drawStr(2, 7, str);
        
        // RSSI
        snprintf(str, sizeof(str), "%ddB", currentRssi);
        u8g2.drawStr(44, 7, str);
        
        // Širokopásmový registr šumu
        snprintf(str, sizeof(str), "WB:%02X", currentWideband);
        u8g2.drawStr(80, 7, str);
        
        // Kosmický vítr indikátor
        if (cosmicWind > 0) u8g2.drawStr(120, 7, ">");
        else if (cosmicWind < 0) u8g2.drawStr(120, 7, "<");
        else u8g2.drawStr(120, 7, "*");
        
    } else if (currentMode == MODE_RF_WATERFALL) {
        u8g2.setFont(u8g2_font_4x6_tf);
        u8g2.setDrawColor(1);
        char str[32];
        float mhz = (float)FREQ_CHANNELS[currentChannelIdx] / 1000000.0f;
        snprintf(str, sizeof(str), "RF SCOPE: %.2fMHz  RSSI:%ddBm", mhz, currentRssi);
        u8g2.drawStr(2, 6, str);
        
    } else if (currentMode == MODE_COSMIC_STATS) {
        // Okno telemetrických statistik
        u8g2.setDrawColor(0);
        u8g2.drawBox(2, 2, 124, 42);
        u8g2.setDrawColor(1);
        u8g2.drawFrame(2, 2, 124, 42);
        
        u8g2.setFont(u8g2_font_5x7_tf);
        u8g2.drawStr(6, 10, "COSMIC ENTROPY STATS");
        
        char buf[32];
        float mhz = (float)FREQ_CHANNELS[currentChannelIdx] / 1000000.0f;
        snprintf(buf, sizeof(buf), "FREQ: %.3f MHz", mhz);
        u8g2.drawStr(6, 19, buf);
        
        snprintf(buf, sizeof(buf), "RSSI: %d dBm | WB:0x%02X", currentRssi, currentWideband);
        u8g2.drawStr(6, 28, buf);
        
        snprintf(buf, sizeof(buf), "ADC: %04d | PKT: %u", adcNoiseVal, packetCount);
        u8g2.drawStr(6, 37, buf);
    }
}

// =============================================================================
// RF SWEEP & ENTROPY SAMPLER
// =============================================================================
void sampleRFEnvironment() {
    static uint32_t lastHopTime = 0;
    static uint32_t lastRssiSample = 0;
    uint32_t now = millis();
    
    // 1. Detekce RF signálu / paketu na frekvenci
    uint8_t irqFlags = readSX1278Register(0x12); // REG_IRQ_FLAGS
    if (irqFlags & 0x50) { // RxDone (0x40) nebo ValidHeader (0x10)
        packetCount++;
        burstFramesLeft = 14; // Záblesk sluneční erupce v ohni!
        writeSX1278Register(0x12, irqFlags); // Vymazat příznaky přerušení
    }
    
    // 2. Širokopásmový šum z registru 0x2C (REG_RSSI_WIDEBAND)
    currentWideband = readSX1278Register(0x2C);
    adcNoiseVal = analogRead(ADC_NOISE_PIN);
    
    // Posun historie osciloskopu
    for (int i = 0; i < SCREEN_WIDTH - 1; i++) {
        noiseHistory[i] = noiseHistory[i + 1];
    }
    noiseHistory[SCREEN_WIDTH - 1] = currentWideband;
    
    // 3. Vmíchání čerstvé fyzikální entropie do generátoru
    entropyState ^= ((uint64_t)currentWideband << 56) 
                  ^ ((uint64_t)(adcNoiseVal & 0x0FFF) << 40)
                  ^ ((uint64_t)esp_random() << 8)
                  ^ (uint64_t)esp_cpu_get_ccount();
    
    // 4. Periodické vzorkování RSSI
    if (now - lastRssiSample > 100) {
        lastRssiSample = now;
        uint8_t rawRssi = readSX1278Register(0x1B); // REG_RSSI_VALUE
        currentRssi = -164 + rawRssi; // SX1278 RSSI kalkulace pro 433 MHz
    }
    
    // 5. Frekvenční skenování v pásmu 433 MHz (každých ~200 ms)
    if (now - lastHopTime > 200) {
        lastHopTime = now;
        currentChannelIdx = (currentChannelIdx + 1) % NUM_CHANNELS;
        
        // Přejít do standby na 5 mikrosekund, změnit frekvenci a zpět do RX_CONTINUOUS
        writeSX1278Register(0x01, 0x80 | 0x01); // Standby
        LoRa.setFrequency(FREQ_CHANNELS[currentChannelIdx]);
        writeSX1278Register(0x01, 0x80 | 0x05); // Zpět do RX_CONTINUOUS
        
        // Kosmický vítr: určen podle rozložení šumu napříč spektrem
        if (currentChannelIdx > NUM_CHANNELS / 2) {
            cosmicWind = (currentWideband > 0x12) ? 1 : 0;
        } else {
            cosmicWind = (currentWideband > 0x12) ? -1 : 0;
        }
    }
}

// =============================================================================
// USER INTERACTION (BOOT BUTTON)
// =============================================================================
void handleButton() {
    static bool lastBtnState = HIGH;
    static uint32_t lastDebounceTime = 0;
    bool reading = digitalRead(BOOT_BUTTON);
    
    if (reading != lastBtnState && (millis() - lastDebounceTime > 60)) {
        lastDebounceTime = millis();
        lastBtnState = reading;
        if (reading == LOW) {
            currentMode = (DisplayMode)((currentMode + 1) % MODE_COUNT);
            burstFramesLeft = 8;
        }
    }
}

// =============================================================================
// LEDC PWM FIRE FLICKER LIGHT
// =============================================================================
void updateFlickerLED() {
    // LED svítí a mihotá se přesně podle energie ohně a rádiového šumu
    int baseBrightness = map(constrain(currentRssi, -125, -60), -125, -60, 15, 160);
    uint8_t flicker = harvestEntropyByte() % 50;
    if (burstFramesLeft > 0) baseBrightness = 255;
    int ledDuty = constrain(baseBrightness + flicker, 0, 255);
    ledcWrite(0, ledDuty);
}

// =============================================================================
// SETUP
// =============================================================================
void setup() {
    Serial.begin(115200);
    delay(400);
    
    Serial.println("\n╔══════════════════════════════════════════╗");
    Serial.println("║       COSMIC RANDOM FIRE GENERATOR       ║");
    Serial.println("║    LilyGo LoRa32 T3 V1.6.1 - 433 MHz     ║");
    Serial.println("╚══════════════════════════════════════════╝");
    
    // PWM pro stavovou LED
    ledcSetup(0, 5000, 8);
    ledcAttachPin(BOARD_LED, 0);
    ledcWrite(0, 100);
    
    pinMode(BOOT_BUTTON, INPUT_PULLUP);
    pinMode(ADC_NOISE_PIN, INPUT);
    
    // OLED SSD1306 inicializace
    Wire.begin(OLED_SDA, OLED_SCL);
    u8g2.begin();
    u8g2.setBusClock(800000); // 800 kHz I2C
    
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_ncenB08_tr);
    u8g2.drawStr(12, 22, "COSMIC NOISE");
    u8g2.drawStr(24, 38, "RANDOM FIRE");
    u8g2.setFont(u8g2_font_4x6_tf);
    u8g2.drawStr(10, 56, "LilyGo T3 433MHz SX1278");
    u8g2.sendBuffer();
    
    // LoRa SX1278 inicializace
    SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_CS);
    LoRa.setPins(LORA_CS, LORA_RST, LORA_IRQ);
    
    if (!LoRa.begin(FREQ_CHANNELS[currentChannelIdx])) {
        Serial.println("[!] LoRa SX1278 initialization FAILED!");
        u8g2.setDrawColor(0);
        u8g2.drawBox(0, 48, 128, 16);
        u8g2.setDrawColor(1);
        u8g2.drawStr(20, 60, "LoRa FAILED!");
        u8g2.sendBuffer();
        delay(2000);
    } else {
        Serial.println("[✓] LoRa SX1278 initialized on 433 MHz!");
        LoRa.setSignalBandwidth(500E3); // Široké pásmo pro maximální sběr šumu
        LoRa.setGain(6);                // Maximální citlivost
        LoRa.receive();                 // Režim RX_CONTINUOUS
    }
    
    initFire();
    delay(500);
}

// =============================================================================
// MAIN LOOP
// =============================================================================
void loop() {
    static uint32_t frameCount = 0;
    static uint32_t lastFpsCalc = 0;
    
    // 1. Sběr rádiového a kosmického šumu
    sampleRFEnvironment();
    
    // 2. Tlačítko BOOT
    handleButton();
    
    // 3. Fyzika ohně a vykreslení do bufferu
    updateFirePhysics();
    renderFireToBuffer();
    
    // 4. Překryvné vrstvy režimů
    if (currentMode == MODE_RF_WATERFALL) {
        renderWaterfallMode();
    }
    drawHUD();
    
    // 5. Odeslání do OLED
    u8g2.sendBuffer();
    
    // 6. Mihotavá LED
    updateFlickerLED();
    
    // Měření FPS a telemetrie
    frameCount++;
    uint32_t now = millis();
    if (now - lastFpsCalc >= 1000) {
        fps = (float)frameCount * 1000.0f / (float)(now - lastFpsCalc);
        frameCount = 0;
        lastFpsCalc = now;
        Serial.printf("[COSMIC FIRE] FPS: %.1f | Freq: %.2f MHz | RSSI: %d dBm | Wideband: 0x%02X | Mode: %d\n",
                      fps,
                      (float)FREQ_CHANNELS[currentChannelIdx] / 1000000.0f,
                      currentRssi,
                      currentWideband,
                      (int)currentMode);
    }
}
