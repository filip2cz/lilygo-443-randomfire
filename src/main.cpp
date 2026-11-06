/*******************************************************************************
 * COSMIC RANDOM FIRE - LilyGo T3 LoRa32 V1.6.1 (433MHz)
 * 
 * Snímá náhodné rušení z okolního elektromagnetického prostředí, kosmický šum,
 * atmosférické výboje a fluktuace na frekvenci 433 MHz pomocí transceivery SX1278
 * a ESP32 hardware TRNG/ADC.
 * Na OLED displeji (SSD1306) v reálném čase vykresluje plně stochastický,
 * fyzikálně ditherovaný oheň a plazmu ovlivňovanou zachyceným šumem.
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
#define OLED_RST        U8X8_PIN_NONE // GPIO 16 is NOT used (internally SPI flash on PICO-D4)

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

// Display instance (Full frame buffer in RAM for high-speed direct manipulation)
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, OLED_RST, OLED_SCL, OLED_SDA);

// =============================================================================
// 8x8 BAYER DITHERING MATRIX (Ordered dithering for smooth 256-level gradient)
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
    433050000L, // Low band edge
    433175000L, // LoRa Channel 1
    433375000L, // LoRa Channel 2
    433575000L, // LoRa Channel 3
    433775000L, // LoRa Channel 4
    433920000L, // Center ISM (Car remotes, weather stations, sensors, RF noise peak)
    434175000L, // High LoRa
    434420000L, // Weather / telemeters
    434650000L  // High band edge
};
static const uint8_t NUM_CHANNELS = sizeof(FREQ_CHANNELS) / sizeof(FREQ_CHANNELS[0]);

// =============================================================================
// SPARK PARTICLE SYSTEM (Ejected embers from intense noise events)
// =============================================================================
#define MAX_SPARKS 32
struct Spark {
    float x;
    float y;
    float vx;
    float vy;
    uint8_t life;
    uint8_t maxLife;
    bool active;
};

// =============================================================================
// GLOBAL STATE & TELEMETRY
// =============================================================================
enum DisplayMode {
    MODE_FIRE_HUD = 0,     // Full flame with transparent HUD overlay
    MODE_FIRE_ONLY = 1,    // Pure fullscreen roaring fire (0% text)
    MODE_RF_WATERFALL = 2, // Live RF noise oscilloscope/waterfall + fire base
    MODE_COSMIC_STATS = 3, // Real-time noise entropy statistics & fire monitor
    MODE_COUNT = 4
};

DisplayMode currentMode = MODE_FIRE_HUD;

// Telemetry values
int currentRssi = -120;
uint8_t currentWideband = 0;
uint8_t currentChannelIdx = 5; // Start at 433.92 MHz
uint32_t entropyCount = 0;
uint32_t packetCount = 0;
uint16_t adcNoiseVal = 0;
int cosmicWind = 0;             // Wind drift (-1, 0, +1)
uint8_t burstFramesLeft = 0;    // Triggered when packet or huge RF spike is captured
float fps = 0.0f;

// History buffer for waterfall / noise visualization (128 columns)
uint8_t noiseHistory[SCREEN_WIDTH];

// 2D Heat Buffers for Fire Simulation
uint8_t heat[SCREEN_HEIGHT][SCREEN_WIDTH];
Spark sparks[MAX_SPARKS];

// 64-bit Chaotic Entropy State stirred continuously by physical noise
uint64_t entropyState = 0x1337BEEFDEADBEEFULL;

// Fast inline chaotic generator (1 CPU cycle per byte)
static inline uint8_t harvestEntropyByte() {
    entropyCount++;
    entropyState = entropyState * 6364136223846793005ULL + 1442695040888963407ULL;
    return (uint8_t)(entropyState >> 56);
}

// Low-level SX1278 SPI register read
uint8_t readSX1278Register(uint8_t reg) {
    digitalWrite(LORA_CS, LOW);
    SPI.transfer(reg & 0x7F);
    uint8_t val = SPI.transfer(0x00);
    digitalWrite(LORA_CS, HIGH);
    return val;
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
            sparks[i].y = (float)(SCREEN_HEIGHT - 2);
            sparks[i].vx = ((float)(harvestEntropyByte() % 100) - 50.0f) / 40.0f + ((float)cosmicWind * 0.4f);
            sparks[i].vy = -((float)(harvestEntropyByte() % 100) / 35.0f + 1.2f + power * 1.5f);
            sparks[i].life = (uint8_t)(harvestEntropyByte() % 25 + 15);
            sparks[i].maxLife = sparks[i].life;
            break;
        }
    }
}

void updateSparks() {
    for (int i = 0; i < MAX_SPARKS; i++) {
        if (sparks[i].active) {
            sparks[i].x += sparks[i].vx;
            sparks[i].y += sparks[i].vy;
            sparks[i].vy += 0.04f; // Gravity / aerodynamic drag
            sparks[i].vx += ((float)cosmicWind * 0.05f);
            
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
// FIRE SIMULATION ENGINE
// =============================================================================
void initFire() {
    memset(heat, 0, sizeof(heat));
    memset(noiseHistory, 0, sizeof(noiseHistory));
    initSparks();
}

void updateFirePhysics() {
    // 1. Seed the bottom row (Ember / fuel bed) directly from ambient noise
    int bottomRow = SCREEN_HEIGHT - 1;
    
    // Scale RF interference to fuel intensity
    int rfBoost = constrain(map(currentRssi, -125, -60, 0, 90), 0, 90);
    if (burstFramesLeft > 0) {
        rfBoost = 140; // Solar flare / packet explosion!
        burstFramesLeft--;
    }
    
    for (int x = 0; x < SCREEN_WIDTH; x++) {
        uint8_t r = harvestEntropyByte();
        uint8_t localWideband = (x % 2 == 0) ? currentWideband : (currentWideband ^ r);
        
        // Ember generation: baseline simmer + noise spike + wideband modulation
        int fuel = 110 + rfBoost + ((r & 0x7F)) + ((localWideband & 0x3F));
        
        // Occasional intense hotspots
        if ((r > 240) || (burstFramesLeft > 0 && (r > 160))) {
            fuel = 255;
            if (r > 250) {
                spawnSpark(x, (float)rfBoost / 100.0f);
            }
        }
        
        heat[bottomRow][x] = (uint8_t)constrain(fuel, 0, 255);
    }
    
    // Also feed row bottomRow - 1 slightly to thicken fire base
    for (int x = 0; x < SCREEN_WIDTH; x++) {
        if (harvestEntropyByte() > 180) {
            heat[bottomRow - 1][x] = (heat[bottomRow - 1][x] + heat[bottomRow][x]) / 2;
        }
    }

    // 2. Convection & Heat Diffusion upward (Doom-style diffusion with fluid smoothing)
    for (int y = 1; y < SCREEN_HEIGHT; y++) {
        for (int x = 0; x < SCREEN_WIDTH; x++) {
            uint8_t decayEntropy = harvestEntropyByte();
            
            // Decay cooling factor (influenced by ambient RF noise)
            uint8_t decay = (decayEntropy & 3) + 1;
            if (decayEntropy > 220) decay += 2; // Random cold drafts
            
            // Horizontal drift vector (stochastic jitter + cosmic wind bias)
            int drift = ((int)(decayEntropy >> 2) & 3) - 1 + cosmicWind;
            
            int dstX = x + drift;
            if (dstX < 0) dstX = 0;
            if (dstX >= SCREEN_WIDTH) dstX = SCREEN_WIDTH - 1;
            
            int dstY = y - 1;
            
            // Smooth averaging from lower neighbors
            int xLeft  = (x > 0) ? x - 1 : 0;
            int xRight = (x < SCREEN_WIDTH - 1) ? x + 1 : SCREEN_WIDTH - 1;
            
            int avgHeat = (heat[y][xLeft] + 2 * heat[y][x] + heat[y][xRight]) / 4;
            int cooled = avgHeat - decay;
            if (cooled < 0) cooled = 0;
            
            heat[dstY][dstX] = (uint8_t)cooled;
        }
    }
    
    // 3. Update spark particles
    updateSparks();
}

// =============================================================================
// DITHERED FRAMEBUFFER RENDERING (SSD1306)
// =============================================================================
void renderFireToBuffer() {
    uint8_t* pBuf = u8g2.getBufferPtr();
    uint8_t jitter = (harvestEntropyByte() & 7) - 3; // Temporal noise jitter
    
    // 8 Paged horizontal stripes (each page = 8 vertical pixels)
    for (int page = 0; page < 8; page++) {
        int yBase = page * 8;
        for (int x = 0; x < SCREEN_WIDTH; x++) {
            uint8_t byteVal = 0;
            for (int bit = 0; bit < 8; bit++) {
                int y = yBase + bit;
                uint8_t h = heat[y][x];
                
                // 8x8 Bayer dithering threshold
                uint16_t threshold = (pgm_read_byte(&BAYER_8X8[y & 7][x & 7]) << 2) + jitter;
                if (h > threshold) {
                    byteVal |= (1 << bit);
                }
            }
            pBuf[page * SCREEN_WIDTH + x] = byteVal;
        }
    }
    
    // Overlay glowing sparks directly onto buffer
    for (int i = 0; i < MAX_SPARKS; i++) {
        if (sparks[i].active) {
            int sx = (int)sparks[i].x;
            int sy = (int)sparks[i].y;
            if (sx >= 0 && sx < SCREEN_WIDTH && sy >= 0 && sy < SCREEN_HEIGHT) {
                int page = sy / 8;
                int bit = sy % 8;
                pBuf[page * SCREEN_WIDTH + sx] |= (1 << bit);
                // Make prominent sparks 2-pixels high for glow effect
                if (sparks[i].life > (sparks[i].maxLife / 2) && sy > 0) {
                    int pTop = (sy - 1) / 8;
                    int bTop = (sy - 1) % 8;
                    pBuf[pTop * SCREEN_WIDTH + sx] |= (1 << bTop);
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
    
    // Clear top 26 rows for waterfall / oscilloscope
    for (int page = 0; page < 3; page++) {
        for (int x = 0; x < SCREEN_WIDTH; x++) {
            pBuf[page * SCREEN_WIDTH + x] = 0;
        }
    }
    
    // Draw RF noise oscilloscope line
    for (int x = 0; x < SCREEN_WIDTH; x++) {
        // Map noise history to height 2..24
        int y = constrain(map(noiseHistory[x], 0, 255, 23, 3), 2, 23);
        int page = y / 8;
        int bit = y % 8;
        pBuf[page * SCREEN_WIDTH + x] |= (1 << bit);
        // Dotted grid at y=13
        pBuf[(13 / 8) * SCREEN_WIDTH + x] |= (x % 4 == 0 ? (1 << (13 % 8)) : 0);
    }
    
    // Divider line between RF scope and fire
    for (int x = 0; x < SCREEN_WIDTH; x++) {
        pBuf[(25 / 8) * SCREEN_WIDTH + x] |= (1 << (25 % 8));
    }
}

void drawHUD() {
    u8g2.setFontMode(1); // Transparent mode
    
    if (currentMode == MODE_FIRE_HUD) {
        // Sleek top banner box with solid dark background
        u8g2.setDrawColor(0);
        u8g2.drawBox(0, 0, 128, 10);
        u8g2.setDrawColor(1);
        u8g2.drawHLine(0, 10, 128);
        
        u8g2.setFont(u8g2_font_5x7_tf);
        
        // Frequency (e.g. 433.9M)
        char str[32];
        float mhz = (float)FREQ_CHANNELS[currentChannelIdx] / 1000000.0f;
        snprintf(str, sizeof(str), "%.2fM", mhz);
        u8g2.drawStr(2, 8, str);
        
        // RSSI (e.g. -112dBm)
        snprintf(str, sizeof(str), "%ddB", currentRssi);
        u8g2.drawStr(44, 8, str);
        
        // Activity indicator or wind
        snprintf(str, sizeof(str), "WB:%02X", currentWideband);
        u8g2.drawStr(80, 8, str);
        
        // Pulsing star indicating RF entropy live intake
        if ((entropyCount & 0x10)) {
            u8g2.drawPixel(124, 4);
            u8g2.drawPixel(125, 4);
            u8g2.drawPixel(124, 5);
        }
        
    } else if (currentMode == MODE_RF_WATERFALL) {
        u8g2.setFont(u8g2_font_4x6_tf);
        u8g2.setDrawColor(1);
        char str[32];
        float mhz = (float)FREQ_CHANNELS[currentChannelIdx] / 1000000.0f;
        snprintf(str, sizeof(str), "RF OSC: %.2fMHz  RSSI:%ddBm", mhz, currentRssi);
        u8g2.drawStr(2, 6, str);
        
    } else if (currentMode == MODE_COSMIC_STATS) {
        // Telemetry stats screen overlay
        u8g2.setDrawColor(0);
        u8g2.drawBox(2, 2, 124, 44);
        u8g2.setDrawColor(1);
        u8g2.drawFrame(2, 2, 124, 44);
        
        u8g2.setFont(u8g2_font_5x7_tf);
        u8g2.drawStr(6, 11, "COSMIC ENTROPY STATS");
        
        char buf[32];
        float mhz = (float)FREQ_CHANNELS[currentChannelIdx] / 1000000.0f;
        snprintf(buf, sizeof(buf), "FREQ: %.3f MHz", mhz);
        u8g2.drawStr(6, 21, buf);
        
        snprintf(buf, sizeof(buf), "RSSI: %d dBm | WB:0x%02X", currentRssi, currentWideband);
        u8g2.drawStr(6, 30, buf);
        
        snprintf(buf, sizeof(buf), "ADC34: %04d | PKT: %u", adcNoiseVal, packetCount);
        u8g2.drawStr(6, 39, buf);
    }
}

// =============================================================================
// RF SWEEP & ENTROPY SAMPLER
// =============================================================================
void sampleRFEnvironment() {
    static uint32_t lastHopTime = 0;
    static uint32_t lastRssiSample = 0;
    uint32_t now = millis();
    
    // 1. Check for incoming LoRa packet / signal preamble
    int packetSize = LoRa.parsePacket();
    if (packetSize > 0) {
        packetCount++;
        currentRssi = LoRa.packetRssi();
        burstFramesLeft = 18; // Trigger massive cosmic flare!
        
        // Feed packet bytes into entropy
        while (LoRa.available()) {
            entropyState ^= (LoRa.read() << 8);
        }
    }
    
    // 2. High-speed sample of Wideband RSSI register (0x2C)
    currentWideband = LoRa.random(); // RegRssiWideband
    adcNoiseVal = analogRead(ADC_NOISE_PIN);
    
    // Inject physical hardware noise directly into the chaotic generator state
    entropyState ^= ((uint64_t)currentWideband << 56) 
                  ^ ((uint64_t)(adcNoiseVal & 0x0FFF) << 40)
                  ^ ((uint64_t)esp_random() << 8)
                  ^ (uint64_t)esp_cpu_get_ccount();
    
    // Shift waterfall history
    for (int i = 0; i < SCREEN_WIDTH - 1; i++) {
        noiseHistory[i] = noiseHistory[i + 1];
    }
    noiseHistory[SCREEN_WIDTH - 1] = currentWideband ^ (adcNoiseVal & 0xFF);
    
    // 3. Periodic RSSI update
    if (now - lastRssiSample > 80) {
        lastRssiSample = now;
        currentRssi = LoRa.rssi();
    }
    
    // 4. Frequency Hopping across 433 MHz band (every ~180ms)
    if (now - lastHopTime > 180) {
        lastHopTime = now;
        currentChannelIdx = (currentChannelIdx + 1) % NUM_CHANNELS;
        LoRa.setFrequency(FREQ_CHANNELS[currentChannelIdx]);
        
        // Calculate cosmic wind from frequency gradient
        if (currentChannelIdx > NUM_CHANNELS / 2) {
            cosmicWind = (currentWideband > 0x18) ? 1 : 0;
        } else {
            cosmicWind = (currentWideband > 0x18) ? -1 : 0;
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
    
    if (reading != lastBtnState && (millis() - lastDebounceTime > 50)) {
        lastDebounceTime = millis();
        lastBtnState = reading;
        if (reading == LOW) {
            // Button pressed -> Cycle display mode
            currentMode = (DisplayMode)((currentMode + 1) % MODE_COUNT);
            // Quick burst on mode change
            burstFramesLeft = 10;
        }
    }
}

// =============================================================================
// LEDC PWM FIRE FLICKER LIGHT
// =============================================================================
void updateFlickerLED() {
    // LED glows and flickers dynamically in proportion to fire & RF noise
    int baseBrightness = map(currentRssi, -125, -60, 10, 150);
    uint8_t flicker = harvestEntropyByte() % 60;
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
    
    // Setup Onboard LED with PWM flicker
    ledcSetup(0, 5000, 8); // Channel 0, 5 kHz, 8-bit resolution
    ledcAttachPin(BOARD_LED, 0);
    ledcWrite(0, 255);
    
    // Setup BOOT button
    pinMode(BOOT_BUTTON, INPUT_PULLUP);
    
    // Setup floating ADC pin for atmospheric electrostatic noise
    pinMode(ADC_NOISE_PIN, INPUT);
    
    // Init I2C & SSD1306 Display
    Wire.begin(OLED_SDA, OLED_SCL);
    u8g2.begin();
    u8g2.setBusClock(800000); // 800 kHz I2C high-speed transfer
    
    // Splash screen
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_ncenB08_tr);
    u8g2.drawStr(12, 22, "COSMIC NOISE");
    u8g2.drawStr(24, 38, "RANDOM FIRE");
    u8g2.setFont(u8g2_font_4x6_tf);
    u8g2.drawStr(10, 56, "LilyGo T3 433MHz SX1278");
    u8g2.sendBuffer();
    
    // Init LoRa SX1278
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
        LoRa.setSignalBandwidth(500E3); // 500 kHz wideband for maximum noise collection
        LoRa.setGain(6);                // Maximum LNA gain
        LoRa.receive();                 // Continuous receive mode
    }
    
    // Initialize simulation
    initFire();
    delay(800);
}

// =============================================================================
// MAIN LOOP
// =============================================================================
void loop() {
    static uint32_t frameCount = 0;
    static uint32_t lastFpsCalc = 0;
    
    // 1. Harvest environmental RF & cosmic noise
    sampleRFEnvironment();
    
    // 2. Handle button input (mode change)
    handleButton();
    
    // 3. Update physics and render
    updateFirePhysics();
    renderFireToBuffer();
    
    // 4. Overlay active mode graphics
    if (currentMode == MODE_RF_WATERFALL) {
        renderWaterfallMode();
    }
    drawHUD();
    
    // 5. Push frame to OLED over 800kHz I2C
    u8g2.sendBuffer();
    
    // 6. Synchronize LED firelight
    updateFlickerLED();
    
    // Performance calculation
    frameCount++;
    uint32_t now = millis();
    if (now - lastFpsCalc >= 1000) {
        fps = (float)frameCount * 1000.0f / (float)(now - lastFpsCalc);
        frameCount = 0;
        lastFpsCalc = now;
        Serial.printf("[COSMIC FIRE] FPS: %.1f | Freq: %.2f MHz | RSSI: %d dBm | Wideband: 0x%02X | Entropy: %u\n",
                      fps,
                      (float)FREQ_CHANNELS[currentChannelIdx] / 1000000.0f,
                      currentRssi,
                      currentWideband,
                      entropyCount);
    }
}
