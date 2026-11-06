/*******************************************************************************
 * COSMIC RANDOM INTERFERENCE VISUALIZER - LilyGo T3 LoRa32 V1.6.1 (433MHz)
 * 
 * Snímá náhodné rušení z okolního elektromagnetického prostředí, kosmický šum,
 * atmosférické výboje a fluktuace na frekvenci 433 MHz pomocí transceiveru SX1278
 * a ESP32 hardware TRNG/ADC.
 * 
 * Obsahuje 3 přepínatelné vizualizační režimy:
 *   1. VISUAL_TESLA_LIGHTNING - Elektrická plazma / Blesková bouře (Tesla Arc)
 *   2. VISUAL_CYBER_SPECTRUM  - Cyberpunk RF spektrální analyzátor & vektorový skop
 *   3. VISUAL_COSMIC_FIRE     - Původní kosmický oheň (vyladěný Doom Fire)
 * 
 * Mezi režimy lze přepínat:
 *   - Změnou proměnné 'activeVisualMode' níže v kódu
 *   - Stiskem tlačítka BOOT (GPIO 0) na desce
 *******************************************************************************/

#include <Arduino.h>
#include <SPI.h>
#include <LoRa.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <Preferences.h>

// =============================================================================
// HLAVNÍ REŽIM VIZUALIZACE (Lze nastavit zde nebo přepínat tlačítkem RST / Touch / Serial)
//   0 = VISUAL_TESLA_LIGHTNING (Elektrická plazma / Tesla blesky)
//   1 = VISUAL_CYBER_SPECTRUM  (Cyberpunk RF spektrální analyzátor & osciloskop)
//   2 = VISUAL_COSMIC_FIRE     (Původní kosmický oheň - Doom Fire)
// =============================================================================
enum VisualMode {
    VISUAL_TESLA_LIGHTNING = 0,
    VISUAL_CYBER_SPECTRUM  = 1,
    VISUAL_COSMIC_FIRE     = 2,
    VISUAL_COUNT           = 3
};

// Nastavte výchozí režim zde:
VisualMode activeVisualMode = VISUAL_TESLA_LIGHTNING;

// Automatické střídání režimů (v sekundách, 0 = vypnuto)
// Pokud je nastaveno např. 20, režimy se po 20s samy střídají jako galerie!
uint32_t autoCycleIntervalSeconds = 20;
bool autoCycleEnabled = true;

// Persistence do flash paměti (Preferences) a RTC RAM
Preferences prefs;
RTC_DATA_ATTR int rtcSavedMode = -1;

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
// SPARK PARTICLE SYSTEM (Sparks / Plasma embers)
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
// GLOBAL TELEMETRY & STATE
// =============================================================================
int currentRssi = -115;
uint8_t currentWideband = 0;
uint8_t currentChannelIdx = 5; // 433.92 MHz
uint32_t entropyCount = 0;
uint32_t packetCount = 0;
uint16_t adcNoiseVal = 0;
int cosmicWind = 0;             // -1 (doleva), 0, +1 (doprava)
uint8_t burstFramesLeft = 0;    // Počet snímků plazmové erupce
float fps = 0.0f;
float smoothedEnergy = 0.25f;   // 0.05 = ticho, 1.0 = masivní rušení
uint32_t modeBannerTimer = 0;   // Časovač pro zobrazení banneru při přepnutí režimu

// Spektrum pro 9 kanálů (pro Cyberpunk analyzátor)
float channelPower[NUM_CHANNELS] = {0};
float channelPeak[NUM_CHANNELS] = {0};

// Historie pro osciloskop (128 vzorků)
uint8_t noiseHistory[SCREEN_WIDTH];

// Buffer pro oheň (128x64)
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

static inline float harvestEntropyFloat() {
    return (float)harvestEntropyByte() / 255.0f;
}

// =============================================================================
// SPARK EMBER SYSTEM
// =============================================================================
void initSparks() {
    for (int i = 0; i < MAX_SPARKS; i++) {
        sparks[i].active = false;
    }
}

void spawnSpark(int x, int y, float vx, float vy, uint8_t life) {
    for (int i = 0; i < MAX_SPARKS; i++) {
        if (!sparks[i].active) {
            sparks[i].active = true;
            sparks[i].x = (float)x;
            sparks[i].y = (float)y;
            sparks[i].vx = vx;
            sparks[i].vy = vy;
            sparks[i].life = life;
            break;
        }
    }
}

void updateSparks() {
    for (int i = 0; i < MAX_SPARKS; i++) {
        if (sparks[i].active) {
            sparks[i].x += sparks[i].vx;
            sparks[i].y += sparks[i].vy;
            sparks[i].vy += 0.03f;
            
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
// REŽIM 1: ⚡ ELEKTRICKÁ PLAZMA / BLESKOVÁ BOUŘE (TESLA ARC)
// =============================================================================
void drawLightningSegment(int x0, int y0, int x1, int y1, float displace, int depth) {
    if (depth <= 0 || (abs(x1 - x0) <= 2 && abs(y1 - y0) <= 2)) {
        u8g2.drawLine(x0, y0, x1, y1);
        return;
    }
    
    int midX = (x0 + x1) / 2;
    int midY = (y0 + y1) / 2;
    
    // Posun kolmo k trajektorii
    float jitterX = (harvestEntropyFloat() - 0.5f) * displace;
    float jitterY = (harvestEntropyFloat() - 0.5f) * displace;
    midX += (int)jitterX;
    midY += (int)jitterY;
    
    // Vykreslit obě poloviny
    drawLightningSegment(x0, y0, midX, midY, displace * 0.6f, depth - 1);
    drawLightningSegment(midX, midY, x1, y1, displace * 0.6f, depth - 1);
    
    // Náhodné větvení blesku (pravděpodobnost roste s rušením)
    if (depth >= 2 && harvestEntropyFloat() < (0.20f + smoothedEnergy * 0.35f)) {
        int branchX = midX + (int)((midX - x0) * 0.6f + (harvestEntropyFloat() - 0.5f) * displace * 1.2f);
        int branchY = midY + (int)((midY - y0) * 0.6f + (harvestEntropyFloat() - 0.5f) * displace * 1.2f);
        drawLightningSegment(midX, midY, branchX, branchY, displace * 0.45f, depth - 2);
    }
}

void renderTeslaLightning() {
    u8g2.clearBuffer();
    
    // 1. Horní telemetrická lišta
    u8g2.setFont(u8g2_font_5x7_tf);
    u8g2.setDrawColor(1);
    char str[32];
    float mhz = (float)FREQ_CHANNELS[currentChannelIdx] / 1000000.0f;
    snprintf(str, sizeof(str), "TESLA %.2fM", mhz);
    u8g2.drawStr(1, 7, str);
    
    snprintf(str, sizeof(str), "%ddB", currentRssi);
    u8g2.drawStr(70, 7, str);
    
    // Sloupec energie
    int barW = (int)(smoothedEnergy * 24.0f);
    u8g2.drawFrame(102, 1, 25, 7);
    if (barW > 0) u8g2.drawBox(103, 2, barW, 5);
    u8g2.drawHLine(0, 9, 128);
    
    // 2. Centrální elektroda / Tesla cívka dole uprostřed (x=64, y=56)
    int originX = 64;
    int originY = 54;
    
    // Podstavec cívky
    u8g2.drawHLine(50, 63, 29);
    u8g2.drawHLine(54, 61, 21);
    u8g2.drawHLine(57, 59, 15);
    u8g2.drawLine(59, 58, 69, 58);
    // Kulová elektroda
    u8g2.drawDisc(originX, originY, 3);
    u8g2.drawCircle(originX, originY, 5); // Koróna
    
    // 3. Počet blesků podle energie rušení
    // Klid (0.1) -> 1 blesk, Střed (0.4) -> 2-3 blesky, Špička (0.8+) -> 4-6 blesků
    int numBolts = 1;
    if (smoothedEnergy > 0.20f) numBolts = 2;
    if (smoothedEnergy > 0.45f) numBolts = 3;
    if (smoothedEnergy > 0.70f) numBolts = 4;
    if (burstFramesLeft > 0) numBolts = 6;
    
    for (int b = 0; b < numBolts; b++) {
        // Cílový bod výboje na stropě nebo bocích
        int targetX, targetY;
        float angle = (harvestEntropyFloat() * 140.0f - 70.0f) * 0.0174533f; // -70° až +70° od vertikály
        float reach = 20.0f + smoothedEnergy * 42.0f + harvestEntropyFloat() * 15.0f;
        
        targetX = originX + (int)(sin(angle) * reach);
        targetY = originY - (int)(cos(angle) * reach);
        targetX = constrain(targetX, 4, 123);
        targetY = constrain(targetY, 11, 55);
        
        float displace = 12.0f + smoothedEnergy * 16.0f;
        int depth = 4;
        
        drawLightningSegment(originX, originY - 3, targetX, targetY, displace, depth);
        
        // Jiskra na konci výboje
        if (harvestEntropyByte() > 180) {
            spawnSpark(targetX, targetY, (harvestEntropyFloat() - 0.5f) * 1.5f, -harvestEntropyFloat() * 1.5f, 10);
        }
    }
    
    // 4. Aktualizace a vykreslení plazmových částic
    updateSparks();
    for (int i = 0; i < MAX_SPARKS; i++) {
        if (sparks[i].active) {
            int sx = (int)sparks[i].x;
            int sy = (int)sparks[i].y;
            if (sx >= 0 && sx < SCREEN_WIDTH && sy >= 10 && sy < SCREEN_HEIGHT) {
                u8g2.drawPixel(sx, sy);
            }
        }
    }
}

// =============================================================================
// REŽIM 2: 📡 CYBERPUNK RF SPEKTRÁLNÍ ANALYZÁTOR & VEKTOROVÝ OSKILOSKOP
// =============================================================================
void renderCyberSpectrum() {
    u8g2.clearBuffer();
    
    // ─── 1. HORNÍ ČÁST: 9-pásmový sloupcový spektrální analyzátor (y = 0..20) ───
    // 9 kanálů, šířka sloupce 10 px, mezera 3 px -> 9*13 - 3 = 114 px (vycentrováno x = 7..120)
    int startX = 7;
    int maxBarH = 15;
    
    for (int i = 0; i < NUM_CHANNELS; i++) {
        int x = startX + i * 13;
        int h = (int)(channelPower[i] * maxBarH);
        h = constrain(h, 1, maxBarH);
        
        // Vykreslit sloupec
        u8g2.drawBox(x, 18 - h, 10, h);
        
        // Peak Hold marker (horní líně klesající pomaleji)
        int peakY = 18 - (int)(channelPeak[i] * maxBarH);
        peakY = constrain(peakY, 2, 17);
        u8g2.drawHLine(x, peakY, 10);
        
        // Zvýraznění aktuálně naladěného kanálu
        if (i == currentChannelIdx) {
            u8g2.drawPixel(x + 4, 19);
            u8g2.drawPixel(x + 5, 19);
        }
    }
    
    // Dělicí horizontální linie pod spektrem
    u8g2.drawHLine(0, 21, 128);
    
    // ─── 2. STŘEDNÍ ČÁST: Vektorový osciloskop (y = 22..53) ───
    // Rastr / kříž
    for (int x = 0; x < SCREEN_WIDTH; x += 4) {
        u8g2.drawPixel(x, 37); // Středová osa X
    }
    for (int y = 23; y < 53; y += 4) {
        u8g2.drawPixel(64, y); // Středová osa Y
    }
    
    // Spojitá křivka osciloskopu
    int prevX = 0;
    int prevY = 37;
    
    for (int x = 0; x < SCREEN_WIDTH; x++) {
        // Amplituda modulovaná rušením
        float normVal = ((float)noiseHistory[x] - 12.0f) / 24.0f;
        int waveY = 37 - (int)(normVal * (10.0f + smoothedEnergy * 14.0f));
        waveY = constrain(waveY, 23, 52);
        
        if (x > 0) {
            u8g2.drawLine(prevX, prevY, x, waveY);
        }
        prevX = x;
        prevY = waveY;
    }
    
    // Dělicí linie pod skopem
    u8g2.drawHLine(0, 53, 128);
    
    // ─── 3. SPODNÍ ČÁST: Cyberpunk telemetrická páska (y = 55..63) ───
    u8g2.setFont(u8g2_font_4x6_tf);
    char tape[36];
    float mhz = (float)FREQ_CHANNELS[currentChannelIdx] / 1000000.0f;
    snprintf(tape, sizeof(tape), "%.2fM %ddB [0x%02X %04d PKT:%u]",
             mhz, currentRssi, currentWideband, adcNoiseVal, packetCount);
    u8g2.drawStr(1, 61, tape);
}

// =============================================================================
// REŽIM 3: 🔥 PŮVODNÍ KOSMICKÝ OHEŇ (DOOM FIRE ENGINE)
// =============================================================================
void initFire() {
    memset(heat, 0, sizeof(heat));
    memset(noiseHistory, 0, sizeof(noiseHistory));
    initSparks();
}

void updateFirePhysics() {
    int bottomRow = SCREEN_HEIGHT - 1;
    float E = smoothedEnergy;
    
    int baseFuel = (int)(80.0f + 175.0f * E);
    int decayMin = (int)(10.0f - 7.0f * E);
    int decayRange = 3;
    int gapThreshold = (int)(64.0f - 56.0f * E);
    
    // 1. Spodní řádek: Palivo
    for (int x = 0; x < SCREEN_WIDTH; x++) {
        uint8_t r = harvestEntropyByte();
        if (r < gapThreshold) {
            heat[bottomRow][x] = 0;
        } else {
            int fuel = baseFuel + (r & 0x1F);
            heat[bottomRow][x] = (uint8_t)constrain(fuel, 0, 255);
            if (E > 0.6f && r > 248) {
                spawnSpark(x, SCREEN_HEIGHT - 3, ((float)(r % 100) - 50.0f) / 50.0f, -((float)(r % 100) / 40.0f + 1.2f), 25);
            }
        }
    }
    
    // 2. Doom Fire propagace tepla
    for (int y = 1; y < SCREEN_HEIGHT; y++) {
        for (int x = 0; x < SCREEN_WIDTH; x++) {
            uint8_t pixel = heat[y][x];
            if (pixel == 0) {
                if (heat[y - 1][x] > 3) heat[y - 1][x] -= 3;
                else heat[y - 1][x] = 0;
            } else {
                uint8_t r = harvestEntropyByte();
                int decay = decayMin + (r & decayRange);
                int drift = ((int)(r >> 2) & 3) - 1 + cosmicWind;
                
                int dstX = x + drift;
                if (dstX < 0) dstX = 0;
                else if (dstX >= SCREEN_WIDTH) dstX = SCREEN_WIDTH - 1;
                
                int cooled = (int)pixel - decay;
                heat[y - 1][dstX] = (cooled > 0) ? (uint8_t)cooled : 0;
            }
        }
    }
    
    updateSparks();
}

void renderFireToBuffer() {
    uint8_t* pBuf = u8g2.getBufferPtr();
    
    for (int page = 0; page < 8; page++) {
        int yBase = page * 8;
        for (int x = 0; x < SCREEN_WIDTH; x++) {
            uint8_t byteVal = 0;
            for (int bit = 0; bit < 8; bit++) {
                int y = yBase + bit;
                uint8_t h = heat[y][x];
                uint16_t threshold = ((uint16_t)pgm_read_byte(&BAYER_8X8[y & 7][x & 7])) * 4;
                if (h > threshold) {
                    byteVal |= (1 << bit);
                }
            }
            pBuf[page * SCREEN_WIDTH + x] = byteVal;
        }
    }
    
    // Jiskry
    for (int i = 0; i < MAX_SPARKS; i++) {
        if (sparks[i].active) {
            int sx = (int)sparks[i].x;
            int sy = (int)sparks[i].y;
            if (sx >= 0 && sx < SCREEN_WIDTH && sy >= 0 && sy < SCREEN_HEIGHT) {
                pBuf[(sy / 8) * SCREEN_WIDTH + sx] |= (1 << (sy % 8));
            }
        }
    }
}

void drawFireHUD() {
    u8g2.setFontMode(1);
    u8g2.setDrawColor(0);
    u8g2.drawBox(0, 0, 128, 9);
    u8g2.setDrawColor(1);
    u8g2.drawHLine(0, 9, 128);
    
    u8g2.setFont(u8g2_font_5x7_tf);
    char str[32];
    float mhz = (float)FREQ_CHANNELS[currentChannelIdx] / 1000000.0f;
    snprintf(str, sizeof(str), "FIRE %.2fM", mhz);
    u8g2.drawStr(2, 7, str);
    
    snprintf(str, sizeof(str), "%ddB", currentRssi);
    u8g2.drawStr(66, 7, str);
    
    snprintf(str, sizeof(str), "WB:%02X", currentWideband);
    u8g2.drawStr(98, 7, str);
}

// =============================================================================
// MODE SWITCH NOTIFICATION BANNER
// =============================================================================
void drawModeBanner() {
    if (millis() < modeBannerTimer) {
        u8g2.setFontMode(0);
        u8g2.setDrawColor(0);
        u8g2.drawBox(14, 16, 100, 32);
        u8g2.setDrawColor(1);
        u8g2.drawFrame(14, 16, 100, 32);
        u8g2.drawFrame(16, 18, 96, 28);
        
        u8g2.setFont(u8g2_font_6x10_tf);
        if (activeVisualMode == VISUAL_TESLA_LIGHTNING) {
            u8g2.drawStr(22, 29, "TESLA LIGHTNING");
            u8g2.setFont(u8g2_font_4x6_tf);
            u8g2.drawStr(34, 40, "PLASMA ARC MODE");
        } else if (activeVisualMode == VISUAL_CYBER_SPECTRUM) {
            u8g2.drawStr(24, 29, "CYBER SPECTRUM");
            u8g2.setFont(u8g2_font_4x6_tf);
            u8g2.drawStr(32, 40, "RF ANALYZER & SCOPE");
        } else if (activeVisualMode == VISUAL_COSMIC_FIRE) {
            u8g2.drawStr(30, 29, "COSMIC FIRE");
            u8g2.setFont(u8g2_font_4x6_tf);
            u8g2.drawStr(36, 40, "DOOM FLAME ENGINE");
        }
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
    uint8_t irqFlags = readSX1278Register(0x12);
    if (irqFlags & 0x50) {
        packetCount++;
        burstFramesLeft = 14;
        writeSX1278Register(0x12, irqFlags);
    }
    
    // 2. Širokopásmový šum z registru 0x2C (REG_RSSI_WIDEBAND)
    currentWideband = readSX1278Register(0x2C);
    adcNoiseVal = analogRead(ADC_NOISE_PIN);
    
    // Posun historie osciloskopu
    for (int i = 0; i < SCREEN_WIDTH - 1; i++) {
        noiseHistory[i] = noiseHistory[i + 1];
    }
    noiseHistory[SCREEN_WIDTH - 1] = currentWideband;
    
    // 3. Vmíchání čerstvé fyzikální entropie
    entropyState ^= ((uint64_t)currentWideband << 56) 
                  ^ ((uint64_t)(adcNoiseVal & 0x0FFF) << 40)
                  ^ ((uint64_t)esp_random() << 8)
                  ^ (uint64_t)esp_cpu_get_ccount();
    
    // 4. Periodické vzorkování RSSI
    if (now - lastRssiSample > 90) {
        lastRssiSample = now;
        uint8_t rawRssi = readSX1278Register(0x1B);
        currentRssi = -164 + rawRssi;
        
        // Výpočet adaptivní energie ohně a blesků
        float wbNorm = constrain(((float)currentWideband - 4.0f) / 40.0f, 0.0f, 1.0f);
        float rssiNorm = constrain((float)(currentRssi + 80) / 40.0f, 0.0f, 1.0f);
        float targetEnergy = constrain(wbNorm * 0.7f + rssiNorm * 0.3f, 0.05f, 1.0f);
        if (burstFramesLeft > 0) targetEnergy = 1.0f;
        
        float alpha = (targetEnergy > smoothedEnergy) ? 0.08f : 0.03f;
        smoothedEnergy += alpha * (targetEnergy - smoothedEnergy);
        
        // Aktualizace spektrálního analyzátoru pro aktivní kanál
        float p = constrain(wbNorm * 0.6f + rssiNorm * 0.4f, 0.05f, 1.0f);
        channelPower[currentChannelIdx] = channelPower[currentChannelIdx] * 0.5f + p * 0.5f;
        if (channelPower[currentChannelIdx] > channelPeak[currentChannelIdx]) {
            channelPeak[currentChannelIdx] = channelPower[currentChannelIdx];
        }
    }
    
    // Pozvolný pokles peaků ve spektru
    for (int i = 0; i < NUM_CHANNELS; i++) {
        channelPeak[i] = max(0.0f, channelPeak[i] - 0.008f);
    }
    
    // 5. Frekvenční skenování v pásmu 433 MHz (každých ~180 ms)
    if (now - lastHopTime > 180) {
        lastHopTime = now;
        currentChannelIdx = (currentChannelIdx + 1) % NUM_CHANNELS;
        
        writeSX1278Register(0x01, 0x80 | 0x01); // Standby
        LoRa.setFrequency(FREQ_CHANNELS[currentChannelIdx]);
        writeSX1278Register(0x01, 0x80 | 0x05); // Zpět do RX_CONTINUOUS
        
        if (currentChannelIdx > NUM_CHANNELS / 2) {
            cosmicWind = (currentWideband > 0x12) ? 1 : 0;
        } else {
            cosmicWind = (currentWideband > 0x12) ? -1 : 0;
        }
    }
}

// =============================================================================
// REŽIMOVÉ PŘEPÍNÁNÍ (RST tlačítko, Touch pin GPIO 4, Serial, Auto-Cycle)
// =============================================================================
void setVisualMode(VisualMode newMode) {
    activeVisualMode = newMode;
    rtcSavedMode = (int)activeVisualMode;
    prefs.putInt("mode", (int)activeVisualMode);
    modeBannerTimer = millis() + 1600; // Zobrazit banner na 1.6s
    burstFramesLeft = 8;
    Serial.printf("\n[REŽIM PŘEPNUT] -> %d (%s)\n",
                  (int)activeVisualMode,
                  activeVisualMode == VISUAL_TESLA_LIGHTNING ? "TESLA LIGHTNING" :
                  activeVisualMode == VISUAL_CYBER_SPECTRUM  ? "CYBER SPECTRUM" : "COSMIC FIRE");
}

void nextVisualMode() {
    setVisualMode((VisualMode)((activeVisualMode + 1) % VISUAL_COUNT));
}

// 1. Automatické střídání režimů (Carousel)
void handleAutoCycle() {
    static uint32_t lastCycleTime = 0;
    if (autoCycleEnabled && autoCycleIntervalSeconds > 0) {
        if (millis() - lastCycleTime >= autoCycleIntervalSeconds * 1000) {
            lastCycleTime = millis();
            nextVisualMode();
        }
    }
}

// 2. Sériový vstup z terminálu (klávesnice přes USB)
void handleSerialCommands() {
    while (Serial.available()) {
        char c = Serial.read();
        if (c == '1') {
            setVisualMode(VISUAL_TESLA_LIGHTNING);
        } else if (c == '2') {
            setVisualMode(VISUAL_CYBER_SPECTRUM);
        } else if (c == '3') {
            setVisualMode(VISUAL_COSMIC_FIRE);
        } else if (c == ' ' || c == 'm' || c == 'M' || c == 'n' || c == 'N') {
            nextVisualMode();
        } else if (c == 'a' || c == 'A') {
            autoCycleEnabled = !autoCycleEnabled;
            Serial.printf("[AUTO-CYCLE] %s\n", autoCycleEnabled ? "ZAPNUTO" : "VYPNUTO");
        }
    }
}

// 3. Kapacitní dotyk na pin GPIO 4 (header pin na kraji desky)
void handleCapacitiveTouch() {
    static uint32_t lastTouchCheck = 0;
    static uint32_t lastTouchTrigger = 0;
    uint32_t now = millis();
    
    if (now - lastTouchCheck > 40) {
        lastTouchCheck = now;
        uint16_t touchVal = touchRead(4); // Pin T0 / GPIO 4
        // Hodnota při volném pinu bývá ~70-90, při dotyku prstem klesne pod 35
        if (touchVal < 32 && (now - lastTouchTrigger > 600)) {
            lastTouchTrigger = now;
            nextVisualMode();
        }
    }
}

// 4. BOOT tlačítko (pokud je na dané revizi osazeno na GPIO 0)
void handleButton() {
    static bool lastBtnState = HIGH;
    static uint32_t lastDebounceTime = 0;
    bool reading = digitalRead(BOOT_BUTTON);
    
    if (reading != lastBtnState && (millis() - lastDebounceTime > 80)) {
        lastDebounceTime = millis();
        lastBtnState = reading;
        if (reading == LOW) {
            nextVisualMode();
        }
    }
}

// =============================================================================
// LEDC PWM LIGHT (ADAPTIVE LIGHTING)
// =============================================================================
void updateFlickerLED() {
    if (activeVisualMode == VISUAL_TESLA_LIGHTNING) {
        // V režimu Tesla LED pulzuje jako stroboskop při výbojích
        int ledVal = (int)(smoothedEnergy * 180.0f);
        if (harvestEntropyByte() > 220) ledVal = 255;
        ledcWrite(0, constrain(ledVal, 0, 255));
    } else if (activeVisualMode == VISUAL_CYBER_SPECTRUM) {
        // V režimu spektra LED svítí úměrně k signálu na aktuální frekvenci
        int ledVal = (int)(channelPower[currentChannelIdx] * 255.0f);
        ledcWrite(0, constrain(ledVal, 10, 255));
    } else {
        // V režimu ohně plápolá jako světlo krbu
        int baseBrightness = (int)(smoothedEnergy * 160.0f);
        uint8_t flicker = harvestEntropyByte() % 45;
        ledcWrite(0, constrain(baseBrightness + flicker, 0, 255));
    }
}

// =============================================================================
// SETUP
// =============================================================================
void setup() {
    Serial.begin(115200);
    delay(400);
    
    Serial.println("\n╔═════════════════════════════════════════════════╗");
    Serial.println("║    COSMIC RANDOM INTERFERENCE VISUALIZER        ║");
    Serial.println("║       LilyGo LoRa32 T3 V1.6.1 - 433 MHz         ║");
    Serial.println("║  [1] Tesla Arc  [2] Cyber Spectrum  [3] Fire    ║");
    Serial.println("║  Ovládání: RST tlačítko, Touch GPIO 4, Serial   ║");
    Serial.println("╚═════════════════════════════════════════════════╝");
    
    // 1. Správa režimů a detekce tlačítka RST
    prefs.begin("visuals", false);
    esp_reset_reason_t reason = esp_reset_reason();
    
    if (reason == ESP_RST_EXT) {
        // Uživatel stiskl fyzické tlačítko RST na boku desky!
        int prevMode = prefs.getInt("mode", 0);
        int nextMode = (prevMode + 1) % VISUAL_COUNT;
        prefs.putInt("mode", nextMode);
        activeVisualMode = (VisualMode)nextMode;
        modeBannerTimer = millis() + 2500;
        Serial.printf("[RST TLAČÍTKO] Přepnuto na režim: %d\n", nextMode);
    } else {
        // Běžné zapnutí / zapojení USB: načíst uložený režim
        int savedMode = prefs.getInt("mode", (int)activeVisualMode);
        activeVisualMode = (VisualMode)savedMode;
        modeBannerTimer = millis() + 1500;
    }
    
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
    u8g2.drawStr(10, 22, "COSMIC SENSOR");
    u8g2.drawStr(22, 38, "RF INTERFERENCE");
    u8g2.setFont(u8g2_font_4x6_tf);
    u8g2.drawStr(12, 56, "Tesla / Spectrum / Fire");
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
        LoRa.setSignalBandwidth(500E3);
        LoRa.setGain(6);
        LoRa.receive();
    }
    
    initFire();
    initSparks();
    delay(400);
}

// =============================================================================
// MAIN LOOP
// =============================================================================
void loop() {
    static uint32_t frameCount = 0;
    static uint32_t lastFpsCalc = 0;
    
    // 1. Sběr rádiového a kosmického šumu
    sampleRFEnvironment();
    
    // 2. Obsluha přepínání: Automatický kolotoč, Sériová linka, Dotyk, Tlačítko
    handleAutoCycle();
    handleSerialCommands();
    handleCapacitiveTouch();
    handleButton();
    
    // 3. Vykreslení podle aktivního vizuálního režimu
    switch (activeVisualMode) {
        case VISUAL_TESLA_LIGHTNING:
            renderTeslaLightning();
            break;
            
        case VISUAL_CYBER_SPECTRUM:
            renderCyberSpectrum();
            break;
            
        case VISUAL_COSMIC_FIRE:
            updateFirePhysics();
            renderFireToBuffer();
            drawFireHUD();
            break;
    }
    
    // 4. Překryvný banner při přepnutí režimu
    drawModeBanner();
    
    // 5. Odeslání snímku do OLED
    u8g2.sendBuffer();
    
    // 6. Hardwarová LED
    updateFlickerLED();
    
    // Telemetrie na Serial
    frameCount++;
    uint32_t now = millis();
    if (now - lastFpsCalc >= 1000) {
        fps = (float)frameCount * 1000.0f / (float)(now - lastFpsCalc);
        frameCount = 0;
        lastFpsCalc = now;
        Serial.printf("[COSMIC] FPS: %.1f | Freq: %.2f MHz | RSSI: %d dBm | WB: 0x%02X | Energy: %.0f%% | Mode: %d | Auto: %s\n",
                      fps,
                      (float)FREQ_CHANNELS[currentChannelIdx] / 1000000.0f,
                      currentRssi,
                      currentWideband,
                      smoothedEnergy * 100.0f,
                      (int)activeVisualMode,
                      autoCycleEnabled ? "ON" : "OFF");
    }
}
