/*
 * Cuantum Life — Holographic Game of Life for Cuantum32
 *
 * A fusion of "Fast Life" (Optimized Conway's Game of Life)
 * and "MasterV2" (I2C Master/Slave Architecture).
 *
 * Concept:
 *  - The OLED "Bulk" runs the Game of Life.
 *  - I2C Slaves "Boundary" act as entropy sources.
 *  - Visualizes the "Holographic Principle": Boundary fluctuations energize the Bulk.
 *
 * Hardware:
 *   - ESP32 (Master)
 *   - OLED: SH1106 (I2C 0x3C)
 *   - Buttons: Pin 2 (A), Pin 1 (B)
 *   - NeoPixel: Pin 48
 *   - SD Card: Custom HSPI Pins (CS=7, MOSI=6, MISO=4, CLK=5)
 *
 * Author: Alejandro Rebolledo (arebolledo@udd.cl)
 * Date: 2025-12-19
 * License: CC BY-NC 4.0
 */

#include <Wire.h>
#include <U8g2lib.h>
#include <Adafruit_NeoPixel.h>
#include "FS.h"
#include "SD.h"
#include "SPI.h"

// ============================================================================
// CONFIGURATION
// ============================================================================
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define I2C_ADDR 0x3C

// Loop/Hash Detection
#define SHORT_HASH_INTERVAL 6
#define LONG_HASH_INTERVAL 256

// Master/Slave
#define NUM_SLAVES 4
static const uint8_t SLAVE_ADDRESSES[] = { 0x10, 0x11, 0x12, 0x13 };
#define I2C_CLOCK_SPEED 100000
#define I2C_TIMEOUT_MS  100
#define I2C_MAX_RX_BYTES 32

// Pins (Cuantum32 Standard)
static constexpr int BTNA_PIN = 2; // Swapped per request (A=2)
static constexpr int BTNB_PIN = 1; // Swapped per request (B=1)
static constexpr int RGB_LED_PIN = 48;

// Custom SD Pins
static constexpr int SD_CS_PIN   = 7;
static constexpr int SD_MOSI_PIN = 6;
static constexpr int SD_MISO_PIN = 4;
static constexpr int SD_SCLK_PIN = 5;

// Timing
#define FRAME_TIME_MS 33    // ~30 FPS target
#define POLL_INTERVAL_MS 50 // Poll slaves every 50ms

// ============================================================================
// GLOBALS
// ============================================================================
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);
Adafruit_NeoPixel rgbLed(1, RGB_LED_PIN, NEO_GRB + NEO_KHZ800);
SPIClass spiSD(HSPI);

bool sdOK = false;

// Game State
uint64_t board[128]; // Max width 128
uint64_t first_col, this_col, last_col;
int currentScale = 2; // 1=128x64, 2=64x32
int boardWidth = 64;
int boardHeight = 32;

uint32_t generation = 0;
uint32_t population = 0;
uint32_t short_check_hash = 0;
uint32_t long_check_hash = 0;
uint32_t start_t = 0;

// Slave State (The "Boundary")
struct SlaveObs {
    bool valid;
    uint32_t lastRxMs;
    float noise;
    uint32_t seed;
    uint32_t nextInjectionMs; // AUTO-GENERATION TIMER
};
SlaveObs slaves[NUM_SLAVES];
uint8_t currentPollIdx = 0;

// Buttons
volatile bool btnA_pressed = false;
volatile bool btnB_pressed = false;
volatile uint32_t last_btn_time = 0;

// Defines for bit counting
static const uint8_t bit_counts[] = { 0, 1, 1, 2, 1, 2, 2, 3 };

// ============================================================================
// INTERRUPTS & HELPERS
// ============================================================================
void IRAM_ATTR isrBtnA() {
    if (millis() - last_btn_time > 200) {
        btnA_pressed = true;
        last_btn_time = millis();
    }
}
void IRAM_ATTR isrBtnB() {
    if (millis() - last_btn_time > 200) {
        btnB_pressed = true;
        last_btn_time = millis();
    }
}

void setLed(uint8_t r, uint8_t g, uint8_t b) {
    rgbLed.setPixelColor(0, rgbLed.Color(r, g, b));
    rgbLed.show();
}

// Log loop detection events to SD
void logEvent(const char* event, uint32_t gen) {
    if (!sdOK) return;
    File f = SD.open("/life_log.txt", FILE_APPEND);
    if (f) {
        f.printf("[%lu] %s Gen:%u Pop:%u\n", millis(), event, gen, population);
        f.close();
    }
}

// ============================================================================
// GAME OF LIFE ENGINE
// ============================================================================
static inline uint32_t rot32(uint64_t x, int n) {
    uint32_t x32 = (uint32_t)x;
    int r = (n == 0) ? 31 : (n - 1);
    return ((x32 >> r) | (x32 << (32 - r))) & 0x7;
}

static inline uint32_t rot64(uint64_t x, int n) {
    int r = (n == 0) ? 63 : (n - 1);
    return ((x >> r) | (x << (64 - r))) & 0x7;
}

void drawCell(int x, int y) {
    if (currentScale == 2) {
        // 2x2 Pixel
        u8g2.drawBox(x*2, y*2, 2, 2);
    } else {
        u8g2.drawPixel(x, y);
    }
}

void randomizeBoard() {
    for (int x = 0; x < 128; x++) {
        uint64_t r = ((uint64_t)esp_random() << 32) | esp_random();
        board[x] = r & (((uint64_t)esp_random() << 32) | esp_random()); // ~25% density
        if (currentScale == 2) board[x] &= 0xFFFFFFFF; // Detect 32bit mask
    }
}

void resetGame(bool log = true) {
    if (log) logEvent("RESET", generation);
    Serial.println(">>> GAME RESET <<<");
    randomizeBoard();
    generation = 0;
    short_check_hash = 0;
    long_check_hash = 0;
    setLed(255, 255, 255); // Flash white
    delay(100);
}

// Inject Life from Boundary (Holographic Interaction)
void injectLife(int quadrant, uint32_t seed) {
    // Inject a burst of noise in a specific quadrant
    // Quadrants: 0=TL, 1=TR, 2=BL, 3=BR
    srand(seed);
    
    int w = (currentScale == 1) ? 128 : 64;
    int h = (currentScale == 1) ? 64 : 32;

    int qw = w / 2;
    int qh = h / 2;
    
    int startX = (quadrant % 2) * qw + (qw/4);
    int startY = (quadrant / 2) * qh + (qh/4);
    
    // Serial debug
    Serial.printf("[INJECT] Slave %d (Quad %d) injected life at %d,%d\n", quadrant, quadrant, startX, startY);
    
    // VISUAL FLASH: Draw a box around the injection area momentarily
    u8g2.setDrawColor(2); // XOR mode
    if(currentScale == 2) u8g2.drawBox(startX*2, startY*2, 16, 16);
    else u8g2.drawBox(startX, startY, 16, 16);
    u8g2.setDrawColor(1);
    
    // Inject a 8x8 block of noise
    for(int x=startX; x < startX + 8; x++) {
        if(x >= w) continue;
        uint64_t noise = 0;
        for(int i=0; i<8; i++) {
             if(rand() % 2 == 0) noise |= (1ULL << (startY + i)); // 50% density injection
        }
        board[x] |= noise;
    }
    
    // Flash LED White to indicate external energy
    setLed(100, 100, 100);
}

void evolve() {
    u8g2.clearBuffer();
    uint32_t hash = 5381;
    population = 0;

    for (int x = 0; x < boardWidth; x++) {
        uint64_t left  = (x == 0) ? board[boardWidth - 1] : board[x - 1];
        uint64_t center = board[x];
        uint64_t right = (x == boardWidth - 1) ? board[0] : board[x + 1];
        
        this_col = 0;
        uint32_t hash_acc = 0;
        
        for (int y = 0; y < boardHeight; y++) {
            hash_acc <<= 1;
            uint32_t l, c, r;
            if (currentScale == 2) {
                l = rot32(left, y); c = rot32(center, y); r = rot32(right, y);
            } else {
                l = rot64(left, y); c = rot64(center, y); r = rot64(right, y);
            }
            
            int count = bit_counts[l] + bit_counts[c] + bit_counts[r];
            bool alive = (c & 0x2);
            
            if ((count == 3) || (count == 4 && alive)) {
                this_col |= (1ULL << y);
                hash_acc |= 1;
                population++;
                drawCell(x, y); 
            }
            
            if ((y & 0x7) == 0x7) {
                hash = ((hash << 5) + hash) ^ hash_acc;
                hash_acc = 0;
            }
        }
        
        if (x > 0) board[x - 1] = last_col;
        else first_col = this_col;
        last_col = this_col;
    }
    board[boardWidth - 1] = last_col;
    board[0] = first_col;
    
    // DRAW OVERLAY: Active Slaves (Corner Indicators)
    u8g2.setDrawColor(1);
    if(slaves[0].valid) u8g2.drawDisc(3, 3, 2);   // TL associated with Slave 0
    if(slaves[1].valid) u8g2.drawDisc(124, 3, 2); // TR associated with Slave 1
    if(slaves[2].valid) u8g2.drawDisc(3, 60, 2);  // BL associated with Slave 2
    if(slaves[3].valid) u8g2.drawDisc(124, 60, 2);// BR associated with Slave 3

    u8g2.sendBuffer();

    // Loop Checks
    if (hash == short_check_hash) {
        Serial.println("Short Loop - Holographic Reset");
        resetGame();
    } else if (hash == long_check_hash) {
        // Serial.println("Long Loop - Logging");
        logEvent("LongLoop", generation);
        // Don't reset long loops
    } else {
        if (generation % SHORT_HASH_INTERVAL == 0) short_check_hash = hash;
        if (generation % LONG_HASH_INTERVAL == 0) long_check_hash = hash;
        generation++;
    }
}

// ============================================================================
// I2C SLAVE POLLING
// ============================================================================
void pollSlaves() {
    uint32_t now = millis();
    // Round-robin polling of one slave per call
    uint8_t addr = SLAVE_ADDRESSES[currentPollIdx];
    
    Wire.requestFrom(addr, (uint8_t)I2C_MAX_RX_BYTES);
    if (Wire.available()) {
        char buf[I2C_MAX_RX_BYTES + 1];
        int i = 0;
        while(Wire.available() && i < I2C_MAX_RX_BYTES) buf[i++] = Wire.read();
        buf[i] = 0;

        // Parse: O,tick,bmask,loss,noise,seed
        char type; 
        unsigned long t, s; 
        unsigned int b, l; 
        float n;
        
        int parsed = sscanf(buf, "%c,%lu,%u,%u,%f,%lu", &type, &t, &b, &l, &n, &s);
        if (parsed >= 6 && type == 'O') {
            
            // --- NEW: Init Random Timer if not valid before ---
            if (!slaves[currentPollIdx].valid) {
                 slaves[currentPollIdx].nextInjectionMs = now + (esp_random() % 5000) + 2000;
            }

            slaves[currentPollIdx].valid = true;
            slaves[currentPollIdx].lastRxMs = now;
            slaves[currentPollIdx].noise = n;
            slaves[currentPollIdx].seed = s;
            
            // 1. REACTIVE GENERATION (High Noise = Immediate Life)
            if (n > 0.05f) { 
                int chance = 10 + (int)(n * 80.0);
                if (chance > 90) chance = 90; 
                if ((esp_random() % 100) < chance) {
                    injectLife(currentPollIdx, s + now); 
                }
            }

            // 2. SPONTANEOUS GENERATION (Random Heartbeat)
            // Even in a quiet vacuum, quantum fluctuations create life.
            if (now > slaves[currentPollIdx].nextInjectionMs) {
                Serial.printf("[AUTO] Slave %d spontaneous generation\n", currentPollIdx);
                injectLife(currentPollIdx, s + now + 9999);
                // Reset timer: 2 to 8 seconds
                slaves[currentPollIdx].nextInjectionMs = now + (esp_random() % 6000) + 2000;
            }

        }
    } else {
         slaves[currentPollIdx].valid = false;
    }
    
    currentPollIdx = (currentPollIdx + 1) % NUM_SLAVES;
}

void printDebug() {
    Serial.printf("Gen: %u | Pop: %u | FPS: %u\n", generation, population, 30);
    Serial.print("Slaves: ");
    bool any = false;
    for(int i=0; i<NUM_SLAVES; i++) {
        if(slaves[i].valid) {
            Serial.printf("[%d:OK N=%.2f] ", i, slaves[i].noise);
            any = true;
        }
        else Serial.print("[_] ");
    }
    Serial.println();
    
    if(!any) Serial.println("WARNING: No Slaves Connected.");
}

// ============================================================================
// MAIN SETUP & LOOP
// ============================================================================
void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n=== Cuantum Life (Holographic v1.2: Spontaneous) ===");
    Serial.println("Waiting for I2C Slaves (0x10-0x13)...");
    
    // Hardware Init
    rgbLed.begin(); rgbLed.show();
    pinMode(BTNA_PIN, INPUT_PULLUP); 
    pinMode(BTNB_PIN, INPUT_PULLUP);
    attachInterrupt(BTNA_PIN, isrBtnA, FALLING);
    attachInterrupt(BTNB_PIN, isrBtnB, FALLING);
    
    Wire.begin();
    Wire.setClock(I2C_CLOCK_SPEED);
    
    u8g2.begin();
    
    spiSD.begin(SD_SCLK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);
    sdOK = SD.begin(SD_CS_PIN, spiSD);
    if(sdOK) Serial.println("SD Card: OK");
    else Serial.println("SD Card: FAIL");
    
    // Start Menu
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_ncenB08_tr);
    u8g2.drawStr(0, 10, "Cuantum Life");
    u8g2.drawStr(0, 30, "A: Reset");
    u8g2.drawStr(0, 45, "B: Scale (1/2)");
    u8g2.drawStr(0, 60, sdOK ? "SD: OK" : "SD: FAIL");
    u8g2.sendBuffer();
    delay(2000);
    
    resetGame(false);
}

void loop() {
    static uint32_t lastFrame = 0;
    static uint32_t lastPoll = 0;
    static uint32_t lastDebug = 0;
    
    // 1. Handle Inputs
    if (btnA_pressed) {
        btnA_pressed = false;
        resetGame();
    }
    if (btnB_pressed) {
        btnB_pressed = false;
        currentScale = (currentScale == 1) ? 2 : 1;
        boardWidth = (currentScale == 1) ? 128 : 64;
        boardHeight = (currentScale == 1) ? 64 : 32;
        resetGame();
    }
    
    // 2. Poll Environment (Slaves)
    if (millis() - lastPoll > POLL_INTERVAL_MS) {
        pollSlaves();
        lastPoll = millis();
    }
    
    // 3. Evolve & Draw (Frame Pacing)
    if (millis() - lastFrame > FRAME_TIME_MS) {
        evolve();
        
        // LED Feedback matches Population Density
        // Map 0.0 -> 0.2 Density to Blue -> Red spectrum.
        // Above 0.2 -> Red -> White (Overpopulation)
        
        float density = (float)population / (boardWidth * boardHeight);
        float normalized = density * 5.0f; // Scale 0.2 to 1.0
        
        uint8_t r, g, b_led;
        if(normalized <= 1.0f) {
            // Blue to Red gradient
            // 0.0 = Blue (0,0,255)
            // 0.5 = Purple (128,0,128)
            // 1.0 = Red (255,0,0)
            r = (uint8_t)(normalized * 255);
            b_led = (uint8_t)((1.0f - normalized) * 255);
            g = 0;
        } else {
            // Red to White (Too dense)
            r = 255;
            float over = (normalized - 1.0f); // 0.0 - ...
            if(over > 1.0f) over = 1.0f;
            g = (uint8_t)(over * 255);
            b_led = (uint8_t)(over * 255);
        }
        
        setLed(r, g, b_led);
        lastFrame = millis();
    }

    // 4. Debug Print (Every 1s)
    if (millis() - lastDebug > 1000) {
        printDebug();
        lastDebug = millis();
    }
}
