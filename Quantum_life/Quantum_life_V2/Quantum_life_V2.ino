/* Cuantum Life V2.3.1 — Balanced Holographic Core
 *
 * Fixes vs V2.2:
 * - I2C tick is now independent from Game-of-Life generation (no more expected-tick mismatch).
 * - Non-blocking I2C polling breathers (no delay(4) in the core scheduler).
 * - Health reporting with flags + per-slave status.
 * - Robust retries for BME/SD (no abort in setup).
 * - Non-blocking button handling (no while() wait loops).
 *
 * Hardware (as provided):
 *   BTNA=2, BTNB=1, LED=48, SD(HSPI)=7,6,4,5.
 *
 * Notes:
 * - Keeps your GOL engine + OLED rendering.
 * - Keeps hybrid slave parsing (LONG + COMPACT).
 * - Uses global flags and logs in loop().
 */

#include <Wire.h>
#include <U8g2lib.h>
#include <RTClib.h>
#include <Adafruit_NeoPixel.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include "FS.h"
#include "SD.h"
#include "SPI.h"
#include <esp_random.h>
#include <math.h>

// ============================================================================
// CONFIGURATION & PINS
// ============================================================================
static constexpr int BTNA_PIN = 2;
static constexpr int BTNB_PIN = 1;
static constexpr int RGB_LED_PIN = 48;

// SD (HSPI)
static constexpr int SD_CS_PIN   = 7;
static constexpr int SD_MOSI_PIN = 6;
static constexpr int SD_MISO_PIN = 4;
static constexpr int SD_SCLK_PIN = 5;

// I2C Addresses
static constexpr uint8_t OLED_ADDR = 0x3C;
static constexpr uint8_t BME_ADDR  = 0x76;
static constexpr uint8_t SLAVE_ADDRESSES[] = { 0x10, 0x11, 0x12, 0x13 };
static constexpr uint8_t NUM_SLAVES = sizeof(SLAVE_ADDRESSES) / sizeof(SLAVE_ADDRESSES[0]);

// I2C Settings
static constexpr uint32_t I2C_CLOCK_SPEED  = 100000;
static constexpr uint16_t I2C_TIMEOUT_MS   = 200;
static constexpr uint8_t  I2C_MAX_RX_BYTES = 32;

// Timing (Non-blocking)
static constexpr uint32_t TICK_PERIOD_MS   = 1500;   // I2C tick period
static constexpr uint32_t FRAME_TIME_MS    = 35;     // GOL evolution
static constexpr uint32_t RETRY_PERIOD_MS  = 10000;  // HW retry
static constexpr uint32_t HEALTH_PERIOD_MS = 2000;   // Serial health report
static constexpr uint32_t I2C_BREATHER_MS  = 4;      // gap between slave polls

// Buttons
static constexpr uint32_t BTN_SHORT_MS = 1000;
static constexpr uint32_t BTN_LONG_MS  = 2000;

// ============================================================================
// MODULES & GLOBALS
// ============================================================================
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);
Adafruit_NeoPixel rgbLed(1, RGB_LED_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_BME280 bme;
SPIClass spiSD(HSPI);

// Global flags (no abort)
static bool i2cOK = false;
static bool bmeOK = false;
static bool sdOK  = false;
static bool oledOK = false;

// ============================================================================
// GAME STATE
// ============================================================================
static uint64_t board[128];
static int currentScale = 1; // 1=128x64, 2=64x32

// Game generation (visual evolution) — independent from I2C tick
static uint32_t generation = 0;
static uint32_t population = 0;
static float ambientTemp = 21.0f;
static float globalInjProb = 0.10f;
static bool frozenState = false;

// Loop Detection
#define HASH_BUF_SIZE 32
static uint32_t hashHistory[HASH_BUF_SIZE];
static uint8_t hashIdx = 0;
static bool loopDetected = false;
static uint32_t loopStartTime = 0;

// Simulation Params (shared with slaves)
static float globalNoise = 0.05f;
static int8_t globalBias = 0; // kept for future
static uint8_t globalKp = 0;  // kept for future
static uint8_t simMode = 1;

// ============================================================================
// I2C TICK STATE (robust, independent from GOL generation)
// ============================================================================
static uint32_t tickCounter = 0;  // increments only when starting new I2C tick
static uint32_t activeTick = 0;   // snapshot used for expected parsing
static bool tickInProgress = false;
static uint8_t pollIndex = 0;
static uint32_t lastTickMs = 0;
static uint32_t lastPollStepMs = 0;

// Periodics
static uint32_t nextFrameMs = 0;
static uint32_t nextRetryMs = 0;
static uint32_t lastHealthMs = 0;

// ============================================================================
// SLAVE DATA
// ============================================================================
struct SlaveObs {
  uint32_t tick = 0;
  uint16_t bmask = 0;
  uint16_t loss = 0;
  float noise = NAN;
  uint8_t fmt = 0; // 0=invalid, 1=long, 2=compact
  bool valid = false;
  bool wasValid = false;
  uint32_t lastRxMs = 0;
};

static SlaveObs slaves[NUM_SLAVES];

// ============================================================================
// LED UTILS
// ============================================================================
static void setLed(uint8_t r, uint8_t g, uint8_t b) {
  rgbLed.setPixelColor(0, rgbLed.Color(r, g, b));
  rgbLed.show();
}

// ============================================================================
// INIT HELPERS (NEVER ABORT)
// ============================================================================
static void initI2C() {
  Wire.begin();
  Wire.setClock(I2C_CLOCK_SPEED);
  Wire.setTimeOut(I2C_TIMEOUT_MS);
  i2cOK = true;
}

static void initOLED() {
  if (!oledOK) {
    u8g2.begin();
    oledOK = true;
  }
}

static void initBME() {
  if (!bmeOK) {
    if (bme.begin(BME_ADDR)) {
      bmeOK = true;
      Serial.println("[BME] OK");
    } else {
      Serial.println("[BME] Init failed");
    }
  }
}

static void initSD() {
  if (!sdOK) {
    spiSD.begin(SD_SCLK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);
    if (SD.begin(SD_CS_PIN, spiSD)) {
      sdOK = true;
      Serial.println("[SD] OK");
    } else {
      Serial.println("[SD] Init failed");
    }
  }
}

// ============================================================================
// I2C PROTOCOL (HYBRID: LONG + COMPACT)
// ============================================================================
static inline float clamp01(float x) {
  if (x < 0.0f) return 0.0f;
  if (x > 1.0f) return 1.0f;
  return x;
}

static void sendTickToSlave(uint8_t addr, uint32_t tick) {
  char msg[32];
  snprintf(msg, sizeof(msg), "T,%lu,%u,%.2f\n",
           (unsigned long)tick, (unsigned)simMode, (double)clamp01(globalNoise));

  Wire.beginTransmission(addr);
  Wire.write((const uint8_t*)msg, (uint8_t)strlen(msg));
  uint8_t err = Wire.endTransmission();

  if (err != 0) {
    Serial.printf("[I2C] TX err=%u addr=0x%02X\n", (unsigned)err, (unsigned)addr);
  }
}

static bool parseCompact(const char* buf,
                         uint32_t expectedTick,
                         uint32_t* t,
                         uint16_t* b,
                         uint16_t* l,
                         uint8_t* nB) {
  // "O,%08lX,%04X,%04X,%02X"
  char p = 0;
  unsigned long th = 0;
  unsigned int bh = 0, lh = 0, nh = 0;

  int ok = sscanf(buf, " %c,%lx,%x,%x,%x", &p, &th, &bh, &lh, &nh);
  if (ok < 5 || p != 'O') return false;
  if ((uint32_t)th != expectedTick) return false;

  *t = (uint32_t)th;
  *b = (uint16_t)(bh & 0xFFFFu);
  *l = (uint16_t)(lh & 0xFFFFu);
  *nB = (uint8_t)(nh & 0xFFu);
  return true;
}

static bool parseLong(const char* buf,
                      uint32_t expectedTick,
                      uint32_t* t,
                      uint16_t* b,
                      uint16_t* l,
                      float* n) {
  // "O,<tick>,<bmask>,<loss>,<noise>,<seed>"
  char p = 0;
  unsigned long th = 0;
  unsigned int bh = 0, lh = 0;
  float nf = 0.0f;
  unsigned long seed = 0;

  int ok = sscanf(buf, " %c,%lu,%u,%u,%f,%lu", &p, &th, &bh, &lh, &nf, &seed);
  if (ok < 6 || p != 'O') return false;
  if ((uint32_t)th != expectedTick) return false;

  *t = (uint32_t)th;
  *b = (uint16_t)bh;
  *l = (uint16_t)lh;
  *n = nf;
  return true;
}

static bool readSlave(uint8_t idx, uint32_t expectedTick) {
  uint8_t addr = SLAVE_ADDRESSES[idx];

  uint8_t n = Wire.requestFrom(addr, I2C_MAX_RX_BYTES);
  if (n == 0) return false;

  char buf[I2C_MAX_RX_BYTES + 1];
  uint8_t i = 0;
  while (Wire.available() && i < I2C_MAX_RX_BYTES) {
    buf[i++] = (char)Wire.read();
  }
  buf[i] = '\0';

  uint32_t t = 0;
  uint16_t b = 0, l = 0;
  uint8_t nB = 0;
  float nF = NAN;

  if (parseCompact(buf, expectedTick, &t, &b, &l, &nB)) {
    slaves[idx].tick = t;
    slaves[idx].bmask = b;
    slaves[idx].loss = l;
    slaves[idx].noise = (float)nB / 255.0f;
    slaves[idx].fmt = 2;
    slaves[idx].valid = true;
    slaves[idx].lastRxMs = millis();
    return true;
  }

  if (parseLong(buf, expectedTick, &t, &b, &l, &nF)) {
    slaves[idx].tick = t;
    slaves[idx].bmask = b;
    slaves[idx].loss = l;
    slaves[idx].noise = nF;
    slaves[idx].fmt = 1;
    slaves[idx].valid = true;
    slaves[idx].lastRxMs = millis();
    return true;
  }

  // Optional debug:
  // Serial.printf("[I2C] Parse fail addr=0x%02X exp=%lu got='%s'\n",
  //               (unsigned)addr, (unsigned long)expectedTick, buf);
  return false;
}

static void startNewTick() {
  tickCounter++;
  activeTick = tickCounter;

  tickInProgress = true;
  pollIndex = 0;
  lastPollStepMs = millis();

  for (uint8_t i = 0; i < NUM_SLAVES; i++) {
    slaves[i].wasValid = slaves[i].valid;
    slaves[i].valid = false;
    slaves[i].fmt = 0;
  }

  for (uint8_t i = 0; i < NUM_SLAVES; i++) {
    sendTickToSlave(SLAVE_ADDRESSES[i], activeTick);
  }

  Serial.printf("[I2C] Tick start=%lu\n", (unsigned long)activeTick);
}

// ============================================================================
// GAME ENGINE
// ============================================================================
static void saveSVG() {
  if (!sdOK) return;

  char filename[32];
  snprintf(filename, sizeof(filename), "/life_%lu.svg", (unsigned long)generation);

  File f = SD.open(filename, FILE_WRITE);
  if (!f) return;

  f.println("<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?>");
  f.printf("<svg width=\"%d\" height=\"%d\" xmlns=\"http://www.w3.org/2000/svg\">\n", 128 * 4, 64 * 4);
  f.println("<rect width=\"100%\" height=\"100%\" fill=\"black\"/>");

  for (int x = 0; x < 128; x++) {
    for (int y = 0; y < 64; y++) {
      if (board[x] & (1ULL << y)) {
        f.printf("<rect x=\"%d\" y=\"%d\" width=\"3\" height=\"3\" fill=\"#00FF00\"/>\n", x * 4, y * 4);
      }
    }
  }

  f.println("</svg>");
  f.close();

  setLed(255, 255, 0);
  Serial.printf("[SD] Saved %s\n", filename);
}

static void randomizeBoard() {
  for (int x = 0; x < 128; x++) {
    uint64_t a = ((uint64_t)esp_random() << 32) | (uint64_t)esp_random();
    uint64_t b = ((uint64_t)esp_random() << 32) | (uint64_t)esp_random();
    board[x] = a ^ b;
    if (currentScale == 2) board[x] &= 0x00000000FFFFFFFFULL;
  }
}

static void resetGame() {
  randomizeBoard();
  generation = 0;
  population = 0;
  frozenState = false;
  loopDetected = false;

  for (uint8_t i = 0; i < HASH_BUF_SIZE; i++) hashHistory[i] = 0;
  hashIdx = 0;

  setLed(255, 255, 255);
  Serial.println("[GOL] Reset");
}

static void injectLife(int quadrant, uint16_t bmask) {
  int w = 128, h = 64;
  int startX = (quadrant % 2) * (w / 2) + (w / 8) + (esp_random() % 8);
  int startY = (quadrant / 2) * (h / 2) + (h / 8) + (esp_random() % 8);

  // Pattern selection (favoring gliders for equilibrium)
  uint8_t choice = (bmask >> 12) & 0x07;
  static const uint8_t G1[] = { 0x02, 0x01, 0x07 }; // Glider A
  static const uint8_t G2[] = { 0x04, 0x02, 0x07 }; // Glider B
  static const uint8_t B[] = { 0x03, 0x03 };        // Block

  if (choice < 3) { // 37.5% Glider A
    for (int i = 0; i < 3; i++) board[(startX + i) % w] |= ((uint64_t)G1[i] << (startY % 64));
  } else if (choice < 5) { // 25% Glider B
    for (int i = 0; i < 3; i++) board[(startX + i) % w] |= ((uint64_t)G2[i] << (startY % 64));
  } else if (choice == 5) { // 12.5% Block
    for (int i = 0; i < 2; i++) board[(startX + i) % w] |= ((uint64_t)B[i] << (startY % 64));
  } else { // 25% Sparse random (reduced)
    for (int i = 0; i < 4; i++) {
      if (bmask & (1 << (i*3))) {
        int xx = (startX + (i % 2) * 5) % w;
        int yy = (startY + (i / 2) * 5) % 64;
        board[xx] |= (1ULL << yy);
      }
    }
  }
}

static uint32_t getHash() {
  uint32_t h = 2166136261U;
  for (int i = 0; i < 128; i++) {
    const uint8_t* p = (const uint8_t*)&board[i];
    for (int j = 0; j < 8; j++) {
      h ^= p[j];
      h *= 16777619U;
    }
  }
  return h;
}

static void evolve() {
  if (frozenState) return;

  if (bmeOK) {
    ambientTemp = bme.readTemperature();
  }

  globalInjProb = 0.10f + (ambientTemp - 21.0f) * 0.005f;

  int conn = 0;
  for (int i = 0; i < NUM_SLAVES; i++) if (slaves[i].valid) conn++;

  if (conn == 0) globalInjProb = 0.0f;
  else if (globalInjProb < 0.01f) globalInjProb = 0.01f;

  if ((float)population / 8192.0f > 0.20f) globalInjProb = 0.0f;

  uint64_t next[128];
  uint32_t pop = 0;

  for (int x = 0; x < 128; x++) {
    uint64_t L = board[(x == 0) ? 127 : x - 1];
    uint64_t C = board[x];
    uint64_t R = board[(x == 127) ? 0 : x + 1];

    uint64_t Cu = (C >> 1) | (C << 63);
    uint64_t Cd = (C << 1) | (C >> 63);
    uint64_t Lu = (L >> 1) | (L << 63);
    uint64_t Ld = (L << 1) | (L >> 63);
    uint64_t Ru = (R >> 1) | (R << 63);
    uint64_t Rd = (R << 1) | (R >> 63);

    uint64_t s1 = Lu ^ L,  s2 = Lu & L;
    uint64_t s3 = Ld ^ Cu, s4 = Ld & Cu;
    uint64_t s5 = Cd ^ Ru, s6 = Cd & Ru;
    uint64_t s7 = R ^ Rd,  s8 = R & Rd;

    uint64_t a1 = s1 ^ s3;
    uint64_t a2 = (s1 & s3) ^ s2 ^ s4;
    uint64_t a3 = s2 & s4;

    uint64_t b1 = s5 ^ s7;
    uint64_t b2 = (s5 & s7) ^ s6 ^ s8;
    uint64_t b3 = s6 & s8;

    uint64_t sum1 = a1 ^ b1;
    uint64_t sum2 = (a1 & b1) ^ a2 ^ b2;
    uint64_t sum4 = (a2 & b2) | ((a1 & b1) & (a2 ^ b2)) | a3 | b3;

    uint64_t next_col = (sum2 & (sum1 | C)) & ~sum4;

    // Injection influenced by slaves validity (quadrant mapping kept from your code)
    // Injection (Gravity) Influenced by slaves — SIGNIFICANTLY THROTTLED
    if (globalInjProb > 0.001f) {
      int qX = (x < 64) ? 0 : 1;
      // 20x less noise than V2.3, only single-pixel mutations
      if (slaves[qX].valid && (esp_random() % 1000 < (int)(globalInjProb * 10.0f))) {
        next_col |= (1ULL << (esp_random() % 32));
      }
      if (slaves[qX + 2].valid && (esp_random() % 1000 < (int)(globalInjProb * 10.0f))) {
        next_col |= (1ULL << (32 + (esp_random() % 32)));
      }
    }

    next[x] = next_col;
  }

  if (oledOK) u8g2.clearBuffer();

  for (int x = 0; x < 128; x++) {
    board[x] = next[x];
    for (int y = 0; y < 64; y++) {
      if (board[x] & (1ULL << y)) {
        if (oledOK) {
          if (currentScale == 1) u8g2.drawPixel(x, y);
          else if (x < 64 && y < 32) u8g2.drawBox(x * 2, y * 2, 2, 2);
        }
        pop++;
      }
    }
  }

  population = pop;
  generation++;

  uint32_t h = getHash();
  bool found = false;
  for (int i = 0; i < HASH_BUF_SIZE; i++) if (hashHistory[i] == h) found = true;

  hashHistory[hashIdx] = h;
  hashIdx = (hashIdx + 1) % HASH_BUF_SIZE;

  if (found && generation > 50 && !loopDetected) {
    loopDetected = true;
    loopStartTime = millis();
  }

  if (oledOK) {
    for (int i = 0; i < NUM_SLAVES; i++) {
      if (slaves[i].valid) {
        int ix = (i % 2) ? 124 : 4;
        int iy = (i / 2) ? 55 : 8;
        u8g2.drawDisc(ix, iy, 2);
        if (millis() % 1000 < 200) u8g2.drawCircle(ix, iy, 4);
      }
    }

    u8g2.setDrawColor(1);
    u8g2.setFont(u8g2_font_4x6_tr);
    u8g2.setCursor(0, 63);
    u8g2.printf("GEN:%lu POP:%lu T:%.1fC INJ:%d%%",
                (unsigned long)generation,
                (unsigned long)population,
                (double)ambientTemp,
                (int)(globalInjProb * 100.0f));

    if (loopDetected) {
      int r = 10 - (int)((millis() - loopStartTime) / 1000);
      u8g2.drawBox(30, 25, 70, 15);
      u8g2.setDrawColor(0);
      u8g2.setCursor(35, 35);
      u8g2.printf("LOOP DETECT: %ds", r);
      u8g2.setDrawColor(1);

      setLed((millis() % 500 < 250) ? 255 : 50, 0, 0);
      if (r <= 0) resetGame();
    }

    if (frozenState) {
      u8g2.drawBox(20, 25, 90, 15);
      u8g2.setDrawColor(0);
      u8g2.setCursor(25, 35);
      u8g2.print("FROZEN STATE");
      u8g2.setDrawColor(1);
    }

    u8g2.sendBuffer();
  }
}

// ============================================================================
// HEALTH REPORTING
// ============================================================================
static void printHealth() {
  Serial.printf("=== HEALTH === I2C:%s OLED:%s BME:%s SD:%s tickInProg:%u tick=%lu gen=%lu pop=%lu\n",
                i2cOK ? "OK" : "X",
                oledOK ? "OK" : "X",
                bmeOK ? "OK" : "X",
                sdOK ? "OK" : "X",
                (unsigned)tickInProgress,
                (unsigned long)tickCounter,
                (unsigned long)generation,
                (unsigned long)population);

  for (uint8_t i = 0; i < NUM_SLAVES; i++) {
    Serial.printf("Slave[%u] addr=0x%02X valid:%u fmt:%u loss:%u noise:%.3f lastRx:%lums expTick:%lu\n",
                  (unsigned)i,
                  (unsigned)SLAVE_ADDRESSES[i],
                  (unsigned)slaves[i].valid,
                  (unsigned)slaves[i].fmt,
                  (unsigned)slaves[i].loss,
                  (double)(isnan(slaves[i].noise) ? 0.0f : slaves[i].noise),
                  (unsigned long)(millis() - slaves[i].lastRxMs),
                  (unsigned long)activeTick);
  }
}

// ============================================================================
// BUTTONS (NON-BLOCKING)
// BTNA short: reset game
// BTNA long : save SVG
// BTNB press: toggle scale
// ============================================================================
static bool btnAWasDown = false;
static uint32_t btnADownMs = 0;
static bool btnALongFired = false;

static bool lastB = true;

static void buttonsStep(uint32_t nowMs) {
  bool btnA = (digitalRead(BTNA_PIN) == LOW);
  bool btnB = (digitalRead(BTNB_PIN) == LOW);

  // --- Button A ---
  if (btnA && !btnAWasDown) {
    btnAWasDown = true;
    btnADownMs = nowMs;
    btnALongFired = false;
  }

  if (btnA && btnAWasDown && !btnALongFired && (nowMs - btnADownMs >= BTN_LONG_MS)) {
    btnALongFired = true;
    saveSVG();
  }

  if (!btnA && btnAWasDown) {
    uint32_t held = nowMs - btnADownMs;
    if (!btnALongFired && held < BTN_SHORT_MS) {
      resetGame();
    }
    btnAWasDown = false;
  }

  // --- Button B (edge detect) ---
  if (btnB && lastB) {
    currentScale = (currentScale == 1) ? 2 : 1;
    setLed(0, 255, 255);
    Serial.printf("[BTN] Scale=%d\n", currentScale);
  }
  lastB = !btnB ? true : false;
}

// ============================================================================
// SETUP & LOOP
// ============================================================================
void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("[SYSTEM] Cuantum Life V2.3 Boot");

  pinMode(BTNA_PIN, INPUT_PULLUP);
  pinMode(BTNB_PIN, INPUT_PULLUP);

  rgbLed.begin();
  rgbLed.setBrightness(50);
  rgbLed.show();
  setLed(255, 0, 255);

  // Init (never abort)
  initI2C();
  initOLED();
  initBME();
  initSD();

  resetGame();

  lastTickMs = millis();
  nextFrameMs = millis();
  nextRetryMs = millis() + RETRY_PERIOD_MS;
  lastHealthMs = millis();

  Serial.println("[SYSTEM] V2.3 Robust Core Online");
}

void loop() {
  uint32_t now = millis();

  // Buttons (non-blocking)
  buttonsStep(now);

  // Start new I2C tick periodically (if not already polling)
  if (!tickInProgress && (now - lastTickMs >= TICK_PERIOD_MS)) {
    lastTickMs = now;
    startNewTick();
  }

  // Poll slaves stepwise, with non-blocking breather
  if (tickInProgress) {
    if (now - lastPollStepMs >= I2C_BREATHER_MS) {
      lastPollStepMs = now;

      bool ok = readSlave(pollIndex, activeTick);
      if (ok) {
        if (!slaves[pollIndex].wasValid) {
          setLed(255, 255, 255);
          Serial.printf("[I2C] Slave linked addr=0x%02X fmt=%u\n",
                        (unsigned)SLAVE_ADDRESSES[pollIndex],
                        (unsigned)slaves[pollIndex].fmt);
        }
        // Stricter noise threshold and probabilistic pattern injection
        if (globalInjProb > 0.001f && !isnan(slaves[pollIndex].noise) && slaves[pollIndex].noise > 0.65f) {
          if (esp_random() % 100 < 40) injectLife(pollIndex, slaves[pollIndex].bmask);
        }
      }

      pollIndex++;
      if (pollIndex >= NUM_SLAVES) {
        tickInProgress = false;
        Serial.printf("[I2C] Tick done=%lu\n", (unsigned long)activeTick);
      }
    }
  }

  // Frame scheduler for Game of Life
  if ((int32_t)(now - nextFrameMs) >= 0) {
    evolve();

    float d = (float)population / 8192.0f;
    if (d < 0.05f) setLed(0, 0, 255);
    else if (d < 0.12f) setLed(0, 255, 0);
    else setLed(255, 0, 0);

    nextFrameMs = now + FRAME_TIME_MS;
  }

  // Periodic health report
  if (now - lastHealthMs >= HEALTH_PERIOD_MS) {
    lastHealthMs = now;
    printHealth();
  }

  // Periodic retries (non-spam)
  if (now >= nextRetryMs) {
    if (!bmeOK) initBME();
    if (!sdOK) initSD();
    nextRetryMs = now + RETRY_PERIOD_MS;
  }
}
