/*
 * masterV2.ino — Toy Holography Master (Non-Blocking)
 *
 * Boundary (slaves) -> Bulk reconstruction (master)
 * Pedagogical I2C text protocol + OLED + SD logging
 *
 * Author: Alejandro Rebolledo (arebolledo@udd.cl)
 * Date: 2025-12-16
 * License: CC BY-NC 4.0
 */

#include <Wire.h>
#include <U8g2lib.h>
#include <RTClib.h>
#include <Adafruit_NeoPixel.h>
#include "FS.h"
#include "SD.h"
#include "SPI.h"
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

// ============================================================================
// PIN DEFINITIONS (KEEP AS CURRENT WORKING HARDWARE)
// ============================================================================

// Buttons
static constexpr int BTNA_PIN = 1;  // keep as-is
static constexpr int BTNB_PIN = 2;  // keep as-is

// NeoPixel
static constexpr int RGB_LED_PIN = 48;
static constexpr uint8_t NUM_RGB_LEDS = 1;

// SD (HSPI)
static constexpr int SD_CS_PIN   = 7;
static constexpr int SD_MOSI_PIN = 6;
static constexpr int SD_MISO_PIN = 4;
static constexpr int SD_SCLK_PIN = 5;

// ============================================================================
// I2C ADDRESSES
// ============================================================================
static constexpr uint8_t OLED_ADDR = 0x3C;
static constexpr uint8_t RTC_ADDR  = 0x68;
static constexpr uint8_t BME_ADDR  = 0x76;

// Slaves
static constexpr uint8_t SLAVE_ADDRESSES[] = { 0x10, 0x11, 0x12, 0x13 };
static constexpr uint8_t NUM_SLAVES = sizeof(SLAVE_ADDRESSES) / sizeof(SLAVE_ADDRESSES[0]);

// ============================================================================
// I2C SETTINGS
// ============================================================================
static constexpr uint32_t I2C_CLOCK_SPEED = 100000;
static constexpr uint16_t I2C_TIMEOUT_MS  = 100;
static constexpr uint8_t  I2C_MAX_RX_BYTES = 32;

// ============================================================================
// TIMING (NON-BLOCKING SCHEDULER)
// ============================================================================
static constexpr uint32_t TICK_PERIOD_MS       = 1500;  // one simulation tick
static constexpr uint32_t DISPLAY_PERIOD_MS    = 200;   // OLED refresh
static constexpr uint32_t HEALTH_PERIOD_MS     = 2000;  // Serial status / retry init
static constexpr uint32_t SD_RETRY_PERIOD_MS   = 10000; // retry SD init
static constexpr uint32_t BME_RETRY_PERIOD_MS  = 10000; // retry BME init
static constexpr uint32_t RTC_RETRY_PERIOD_MS  = 10000; // retry RTC init
static constexpr uint32_t OLED_RETRY_PERIOD_MS = 10000; // retry OLED init

// If a slave does not answer in a tick, it is marked missing for that tick
// We do NOT block. Next tick will retry.
static constexpr uint8_t SLAVE_READS_PER_LOOP = 1; // poll 1 slave per loop iteration

// ============================================================================
// OLED (U8g2) — using SH1106 1.3" 128x64 by default
// ============================================================================
U8G2_SH1106_128X64_NONAME_F_HW_I2C display(U8G2_R0, U8X8_PIN_NONE);

// ============================================================================
// MODULES
// ============================================================================
RTC_DS3231 rtc;
Adafruit_NeoPixel rgbLed(NUM_RGB_LEDS, RGB_LED_PIN, NEO_GRB + NEO_KHZ800);
SPIClass spiSD(HSPI);
Adafruit_BME280 bme;

// ============================================================================
// GLOBAL FLAGS (NO ABORT IN SETUP)
// ============================================================================
static bool i2cOK  = false;
static bool oledOK = false;
static bool rtcOK  = false;
static bool sdOK   = false;
static bool bmeOK  = false;

// ============================================================================
// SLAVE DATA MODEL (BOUNDARY OBSERVABLES)
// Protocol from slave: "O,<tick>,<bmask>,<loss>,<noise>,<seed>\n"
// ============================================================================
struct SlaveObs {
  uint32_t tick = 0;
  uint16_t bmask = 0;     // slave-local boundary summary mask (toy)
  uint16_t loss = 0;      // simulated losses/errors
  float noise = 0.0f;     // simulated noise level
  uint32_t seed = 0;      // debug/repro
  bool valid = false;
  uint32_t lastRxMs = 0;
};

static SlaveObs slaveObs[NUM_SLAVES];

// ============================================================================
// TOY BULK RECONSTRUCTION
// bulk logicals are reconstructed if boundary_mask satisfies requirements.
// ============================================================================
static constexpr uint8_t N_LOGICALS = 6;

// Requirements: each logical Li requires a subset of boundary slaves.
// boundary_mask bit i corresponds to SLAVE_ADDRESSES[i].
static constexpr uint8_t LOGICAL_REQ[N_LOGICALS] = {
  0b0001, // L0 needs slave0
  0b0011, // L1 needs slave0+slave1
  0b0110, // L2 needs slave1+slave2
  0b1100, // L3 needs slave2+slave3
  0b0101, // L4 needs slave0+slave2
  0b1110  // L5 needs slave1+slave2+slave3
};

static uint8_t boundary_mask = 0;
static uint8_t bulk_mask = 0;
static float recon_ratio = 0.0f;

// ============================================================================
// ENV DATA
// ============================================================================
static float currentTemp = NAN;
static float currentHum  = NAN;

// ============================================================================
// STATE MACHINE
// ============================================================================
static uint32_t tickCounter = 0;
static uint32_t lastTickMs = 0;
static uint32_t lastDisplayMs = 0;
static uint32_t lastHealthMs = 0;
static uint32_t lastSdRetryMs = 0;
static uint32_t lastBmeRetryMs = 0;
static uint32_t lastRtcRetryMs = 0;
static uint32_t lastOledRetryMs = 0;

static uint8_t pollIndex = 0;        // which slave we poll next
static uint8_t slavesPolledThisTick = 0;
static bool tickInProgress = false;

// Toy mode parameters
static uint8_t simMode = 1;          // 1 = toy code mode (pedagogical)
static float globalNoise = 0.05f;    // broadcast to slaves

// SD
static constexpr const char* SD_FILENAME = "/datalog.csv";

// ============================================================================
// UTILITIES
// ============================================================================
static inline uint8_t popcount8(uint8_t x) {
  uint8_t c = 0;
  while (x) { c += (x & 1); x >>= 1; }
  return c;
}

static void setLedIdle()    { rgbLed.setPixelColor(0, rgbLed.Color(0, 0, 50));  rgbLed.show(); }
static void setLedPolling() { rgbLed.setPixelColor(0, rgbLed.Color(255, 255, 0)); rgbLed.show(); }
static void setLedOk()      { rgbLed.setPixelColor(0, rgbLed.Color(0, 255, 0)); rgbLed.show(); }
static void setLedWarn()    { rgbLed.setPixelColor(0, rgbLed.Color(255, 128, 0)); rgbLed.show(); }
static void setLedErr()     { rgbLed.setPixelColor(0, rgbLed.Color(255, 0, 0)); rgbLed.show(); }

// Timestamp string
static bool getTimestamp(char* out, size_t outLen) {
  if (rtcOK) {
    DateTime now = rtc.now();
    snprintf(out, outLen, "%04d-%02d-%02d %02d:%02d:%02d",
             now.year(), now.month(), now.day(),
             now.hour(), now.minute(), now.second());
    return true;
  }
  // fallback
  snprintf(out, outLen, "ms:%lu", (unsigned long)millis());
  return false;
}

// ============================================================================
// INIT HELPERS (NEVER ABORT)
// ============================================================================
static void initI2C() {
  Wire.begin(); // do not force pins (keep current behavior)
  Wire.setClock(I2C_CLOCK_SPEED);
  Wire.setTimeOut(I2C_TIMEOUT_MS);
  i2cOK = true;
}

static void initOLED() {
  if (!oledOK) {
    // U8g2 begin returns void; we treat as optimistic and verify via bus scan
    display.begin();
    oledOK = true;
  }
}

static void initRTC() {
  if (!rtcOK) {
    if (rtc.begin()) {
      rtcOK = true;
    }
  }
}

static void initSD() {
  if (!sdOK) {
    spiSD.begin(SD_SCLK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);
    if (SD.begin(SD_CS_PIN, spiSD)) {
      sdOK = true;

      // Create file header if missing
      if (!SD.exists(SD_FILENAME)) {
        File f = SD.open(SD_FILENAME, FILE_WRITE);
        if (f) {
          f.println("iso_time,tick,active_slaves,boundary_mask,bulk_mask,recon_ratio,loss_sum,noise_avg,temp,hum");
          f.close();
        }
      }
    }
  }
}

static void initBME() {
  if (!bmeOK) {
    if (bme.begin(BME_ADDR)) {
      bmeOK = true;
    }
  }
}

// ============================================================================
// I2C PROTOCOL
// ============================================================================
static void sendTickToSlave(uint8_t addr, uint32_t tick, uint8_t mode, float noise) {
  // Pedagogical command: "T,<tick>,<mode>,<noise>\n"
  // Keep message short for I2C.
  char msg[32];
  // noise with 2 decimals fits
  snprintf(msg, sizeof(msg), "T,%lu,%u,%.2f\n", (unsigned long)tick, (unsigned)mode, (double)noise);

  Wire.beginTransmission(addr);
  Wire.write((const uint8_t*)msg, (uint8_t)strlen(msg));
  Wire.endTransmission(); // if slave missing, this returns error; we don't block here
}

static bool readObsFromSlave(uint8_t idx, uint8_t addr, uint32_t expectedTick) {
  // Request up to 32 bytes
  uint8_t n = Wire.requestFrom(addr, I2C_MAX_RX_BYTES);
  if (n == 0) return false;

  char buf[I2C_MAX_RX_BYTES + 1];
  uint8_t i = 0;
  while (Wire.available() && i < I2C_MAX_RX_BYTES) {
    buf[i++] = (char)Wire.read();
  }
  buf[i] = '\0';

  // Expected: O,tick,bmask,loss,noise,seed
  // Example: "O,12,15,0,0.05,12345\n"
  char prefix = 0;
  unsigned long tick = 0;
  unsigned int bmask = 0;
  unsigned int loss = 0;
  float noise = 0.0f;
  unsigned long seed = 0;

  int parsed = sscanf(buf, "%c,%lu,%u,%u,%f,%lu", &prefix, &tick, &bmask, &loss, &noise, &seed);
  if (parsed < 6 || prefix != 'O') {
    Serial.printf("[I2C] Parse error from 0x%02X: '%s'\n", addr, buf);
    return false;
  }

  // Tick mismatch is not fatal; we mark invalid for that tick.
  if ((uint32_t)tick != expectedTick) {
    Serial.printf("[I2C] Tick mismatch from 0x%02X: got %lu expected %lu\n",
                  addr, tick, (unsigned long)expectedTick);
    return false;
  }

  slaveObs[idx].tick = (uint32_t)tick;
  slaveObs[idx].bmask = (uint16_t)bmask;
  slaveObs[idx].loss = (uint16_t)loss;
  slaveObs[idx].noise = noise;
  slaveObs[idx].seed = (uint32_t)seed;
  slaveObs[idx].valid = true;
  slaveObs[idx].lastRxMs = millis();

  return true;
}

// ============================================================================
// BULK RECONSTRUCTION (TOY)
// ============================================================================
static void computeBulkFromBoundary() {
  bulk_mask = 0;

  for (uint8_t li = 0; li < N_LOGICALS; li++) {
    uint8_t req = LOGICAL_REQ[li];
    if ((boundary_mask & req) == req) {
      bulk_mask |= (1u << li);
    }
  }

  uint8_t reconCount = popcount8(bulk_mask);
  recon_ratio = (N_LOGICALS > 0) ? (100.0f * reconCount / (float)N_LOGICALS) : 0.0f;
}

// ============================================================================
// SD LOGGING
// ============================================================================
static void logToSD() {
  if (!sdOK) return;

  File f = SD.open(SD_FILENAME, FILE_APPEND);
  if (!f) {
    Serial.println("[SD] Failed to open datalog.csv for append");
    sdOK = false; // force retry later
    return;
  }

  char ts[32];
  getTimestamp(ts, sizeof(ts));

  uint8_t active = popcount8(boundary_mask);

  // aggregate loss + noise avg over valid slaves
  uint32_t lossSum = 0;
  float noiseSum = 0.0f;
  uint8_t validCount = 0;
  for (uint8_t i = 0; i < NUM_SLAVES; i++) {
    if (slaveObs[i].valid) {
      lossSum += slaveObs[i].loss;
      noiseSum += slaveObs[i].noise;
      validCount++;
    }
  }
  float noiseAvg = (validCount > 0) ? (noiseSum / validCount) : NAN;

  // temp/hum update stored in globals
  f.printf("%s,%lu,%u,0x%02X,0x%02X,%.2f,%lu,%.3f,%.2f,%.2f\n",
           ts,
           (unsigned long)tickCounter,
           (unsigned)active,
           (unsigned)boundary_mask,
           (unsigned)bulk_mask,
           (double)recon_ratio,
           (unsigned long)lossSum,
           (double)noiseAvg,
           (double)currentTemp,
           (double)currentHum);

  f.close();
}

// ============================================================================
// OLED DRAW
// ============================================================================
static void drawOLED() {
  if (!oledOK) return;

  display.clearBuffer();

  // Header
  display.setFont(u8g2_font_6x10_tr);
  char line1[32];
  snprintf(line1, sizeof(line1), "T:%lu", (unsigned long)tickCounter);
  display.drawStr(0, 10, line1);

  // Slave status (OK/X)
  display.setFont(u8g2_font_4x6_tr);
  for (uint8_t i = 0; i < NUM_SLAVES && i < 4; i++) {
    int x = 40 + i * 10;
    display.drawStr(x, 10, slaveObs[i].valid ? "OK" : "X");
  }

  // Reconstruction bar (0..100)
  display.setFont(u8g2_font_6x10_tr);
  display.drawStr(0, 24, "Bulk:");
  display.drawFrame(32, 16, 92, 10);
  int barW = (int)(0.92f * recon_ratio); // 92 px max
  if (barW < 0) barW = 0;
  if (barW > 92) barW = 92;
  display.drawBox(32, 16, barW, 10);

  // Ratio text
  char ratioStr[16];
  snprintf(ratioStr, sizeof(ratioStr), "%.0f%%", (double)recon_ratio);
  display.drawStr(100, 24, ratioStr);

  // 2x2 boundary map
  // Cells: 0 1
  //        2 3
  int baseX = 0;
  int baseY = 30;
  int cellW = 28;
  int cellH = 14;
  for (uint8_t i = 0; i < 4; i++) {
    int cx = baseX + (i % 2) * (cellW + 4);
    int cy = baseY + (i / 2) * (cellH + 4);
    display.drawFrame(cx, cy, cellW, cellH);

    if (i < NUM_SLAVES && slaveObs[i].valid) {
      // Fill proportional to (1 - clamp(noise))
      float n = slaveObs[i].noise;
      if (n < 0.0f) n = 0.0f;
      if (n > 1.0f) n = 1.0f;
      int fillW = (int)((1.0f - n) * (cellW - 2));
      if (fillW < 0) fillW = 0;
      display.drawBox(cx + 1, cy + 1, fillW, cellH - 2);
    }

    // Label
    display.setFont(u8g2_font_4x6_tr);
    char lab[8];
    snprintf(lab, sizeof(lab), "S%u", (unsigned)i);
    display.drawStr(cx + cellW - 12, cy + cellH - 3, lab);
  }

  // Footer masks
  display.setFont(u8g2_font_4x6_tr);
  char footer[40];
  snprintf(footer, sizeof(footer), "B:0x%02X K:0x%02X", (unsigned)boundary_mask, (unsigned)bulk_mask);
  display.drawStr(60, 56, footer);

  // Optional env
  if (bmeOK) {
    char env[32];
    snprintf(env, sizeof(env), "T:%.1fC H:%.0f%%", (double)currentTemp, (double)currentHum);
    display.drawStr(0, 56, env);
  }

  display.sendBuffer();
}

// ============================================================================
// HEALTH / SERIAL REPORT
// ============================================================================
static void printHealth() {
  Serial.println("=== MasterV2 Health ===");
  Serial.printf("I2C:%s OLED:%s RTC:%s SD:%s BME:%s\n",
                i2cOK ? "OK" : "X",
                oledOK ? "OK" : "X",
                rtcOK ? "OK" : "X",
                sdOK ? "OK" : "X",
                bmeOK ? "OK" : "X");

  Serial.printf("Tick:%lu BoundaryMask:0x%02X BulkMask:0x%02X Recon:%.1f%%\n",
                (unsigned long)tickCounter,
                (unsigned)boundary_mask,
                (unsigned)bulk_mask,
                (double)recon_ratio);

  for (uint8_t i = 0; i < NUM_SLAVES; i++) {
    Serial.printf("Slave[%u] 0x%02X valid:%u loss:%u noise:%.3f lastRx:%lums\n",
                  (unsigned)i,
                  SLAVE_ADDRESSES[i],
                  (unsigned)slaveObs[i].valid,
                  (unsigned)slaveObs[i].loss,
                  (double)slaveObs[i].noise,
                  (unsigned long)(millis() - slaveObs[i].lastRxMs));
  }
}

// ============================================================================
// TICK CONTROL
// ============================================================================
static void startNewTick() {
  tickCounter++;
  tickInProgress = true;
  slavesPolledThisTick = 0;
  pollIndex = 0;

  // Clear validity for this tick
  for (uint8_t i = 0; i < NUM_SLAVES; i++) {
    slaveObs[i].valid = false;
  }

  // Broadcast tick to all slaves (short, no blocking loops)
  for (uint8_t i = 0; i < NUM_SLAVES; i++) {
    sendTickToSlave(SLAVE_ADDRESSES[i], tickCounter, simMode, globalNoise);
  }

  setLedPolling();
}

// Poll a small amount per loop (non-blocking strategy)
static void pollSlavesStep() {
  uint8_t reads = 0;

  while (reads < SLAVE_READS_PER_LOOP && slavesPolledThisTick < NUM_SLAVES) {
    uint8_t idx = pollIndex;
    uint8_t addr = SLAVE_ADDRESSES[idx];

    bool ok = readObsFromSlave(idx, addr, tickCounter);
    // validity already updated if ok

    pollIndex++;
    slavesPolledThisTick++;
    reads++;

    // If read fails, do nothing else; this tick will mark it missing.
    if (!ok) {
      // keep lastRxMs unchanged; valid stays false
    }
  }

  // If finished polling all slaves, close tick
  if (slavesPolledThisTick >= NUM_SLAVES) {
    // Build boundary_mask
    boundary_mask = 0;
    for (uint8_t i = 0; i < NUM_SLAVES; i++) {
      if (slaveObs[i].valid) boundary_mask |= (1u << i);
    }

    // Reconstruct bulk
    computeBulkFromBoundary();

    // LED status based on responses
    uint8_t validCount = popcount8(boundary_mask);
    if (validCount == NUM_SLAVES) setLedOk();
    else if (validCount == 0) setLedErr();
    else setLedWarn();

    // Read environment once per tick (non-blocking enough)
    if (bmeOK) {
      currentTemp = bme.readTemperature();
      currentHum  = bme.readHumidity();
    }

    // Log to SD once per tick
    logToSD();

    tickInProgress = false;
    setLedIdle();
  }
}

// ============================================================================
// SETUP / LOOP
// ============================================================================
void setup() {
  Serial.begin(115200);
  Serial.println("\n=== MasterV2 (Toy Holography) ===");

  // Buttons (keep pins)
  pinMode(BTNA_PIN, INPUT_PULLUP);
  pinMode(BTNB_PIN, INPUT_PULLUP);

  // NeoPixel (non-fatal)
  rgbLed.begin();
  rgbLed.setBrightness(50);
  rgbLed.show();
  setLedIdle();

  // I2C
  initI2C();

  // Try init peripherals (do not block on failure)
  initOLED();
  initRTC();
  initSD();
  initBME();

  lastTickMs = millis();
  lastDisplayMs = millis();
  lastHealthMs = millis();

  Serial.println("[MasterV2] Setup complete (non-blocking).");
}

void loop() {
  uint32_t nowMs = millis();

  // Buttons (non-blocking)
  if (digitalRead(BTNA_PIN) == LOW) {
    // Example: increase noise
    globalNoise += 0.01f;
    if (globalNoise > 1.0f) globalNoise = 1.0f;
    Serial.printf("[BTN] A pressed -> noise=%.2f\n", (double)globalNoise);
    delay(200);
  }
  if (digitalRead(BTNB_PIN) == LOW) {
    // Example: decrease noise
    globalNoise -= 0.01f;
    if (globalNoise < 0.0f) globalNoise = 0.0f;
    Serial.printf("[BTN] B pressed -> noise=%.2f\n", (double)globalNoise);
    delay(200);
  }

  // Tick scheduler
  if (!tickInProgress && (nowMs - lastTickMs >= TICK_PERIOD_MS)) {
    lastTickMs = nowMs;
    startNewTick();
  }

  // Poll slaves incrementally if tick active
  if (tickInProgress) {
    pollSlavesStep();
  }

  // Display refresh
  if (oledOK && (nowMs - lastDisplayMs >= DISPLAY_PERIOD_MS)) {
    lastDisplayMs = nowMs;
    drawOLED();
  }

  // Health print + retries
  if (nowMs - lastHealthMs >= HEALTH_PERIOD_MS) {
    lastHealthMs = nowMs;
    printHealth();
  }

  // Periodic retry init (do not spam)
  if (!sdOK && (nowMs - lastSdRetryMs >= SD_RETRY_PERIOD_MS)) {
    lastSdRetryMs = nowMs;
    Serial.println("[SD] Retry init...");
    initSD();
  }
  if (!bmeOK && (nowMs - lastBmeRetryMs >= BME_RETRY_PERIOD_MS)) {
    lastBmeRetryMs = nowMs;
    Serial.println("[BME] Retry init...");
    initBME();
  }
  if (!rtcOK && (nowMs - lastRtcRetryMs >= RTC_RETRY_PERIOD_MS)) {
    lastRtcRetryMs = nowMs;
    Serial.println("[RTC] Retry init...");
    initRTC();
  }
  if (!oledOK && (nowMs - lastOledRetryMs >= OLED_RETRY_PERIOD_MS)) {
    lastOledRetryMs = nowMs;
    Serial.println("[OLED] Retry init...");
    initOLED();
  }
}
