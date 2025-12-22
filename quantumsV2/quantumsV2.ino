/*
 * slaveV2.ino — Toy Holography Boundary Node (Non-Blocking)
 *
 * Receives tick commands from master:
 *   "T,<tick>,<mode>,<noise>\n"
 *
 * Responds on request with:
 *   "O,<tick>,<bmask>,<loss>,<noise>,<seed>\n"
 *
 * Pedagogical text protocol, no blocking, no abort in setup.
 *
 * Author: Alejandro Rebolledo (arebolledo@udd.cl)
 * Date: 2025-12-16
 * License: CC BY-NC 4.0
 */

#include <Wire.h>
#include <Arduino.h>

// ============================================================================
// CONFIG — CHANGE PER SLAVE
// ============================================================================
#define SLAVE_ADDRESS 0x11   // change to 0x11, 0x12, 0x13

// Optional LED indicator
static constexpr int LED_PIN = 7;

// ============================================================================
// TIMING (NON-BLOCKING)
// ============================================================================
static constexpr uint32_t WORK_STEP_PERIOD_MS = 20;   // local computation cadence
static constexpr uint32_t HEALTH_PRINT_MS     = 3000; // serial diagnostics

// ============================================================================
// TOY BOUNDARY MODEL
// - boundaryBits represent local "physical" degrees of freedom at the boundary
// - bmask is a summary observable the master can use
// ============================================================================
static constexpr uint8_t BOUNDARY_BITS = 16;

// ============================================================================
// GLOBAL FLAGS
// ============================================================================
static bool i2cOK = false;
static bool ledOK = false;

// ============================================================================
// I2C RX/TX BUFFERS (avoid String in callbacks)
// ============================================================================
static volatile bool tickCmdPending = false;
static volatile uint8_t rxLen = 0;
static char rxBuf[32];  // incoming command buffer, short

static char txBuf[32];  // response buffer prepared in loop (safe)

// ============================================================================
// STATE
// ============================================================================
static uint32_t currentTick = 0;
static uint8_t currentMode = 1;
static float currentNoise = 0.05f;

// Toy boundary state
static uint16_t boundaryState = 0xACE1u; // LFSR-like initial
static uint16_t bmask = 0;
static uint16_t lossCount = 0;
static uint32_t seedNonce = 0;

// Scheduler
static uint32_t lastWorkMs = 0;
static uint32_t lastHealthMs = 0;

// ============================================================================
// UTILS (safe, small)
// ============================================================================
static inline uint16_t lfsr16_step(uint16_t s) {
  // Simple 16-bit LFSR step (toy dynamics)
  // taps: 16,14,13,11 (common)
  uint16_t lsb = s & 1u;
  s >>= 1u;
  if (lsb) s ^= 0xB400u;
  return s;
}

static inline float clamp01(float x) {
  if (x < 0.0f) return 0.0f;
  if (x > 1.0f) return 1.0f;
  return x;
}

// ============================================================================
// I2C CALLBACKS
// ============================================================================
static void onReceiveEvent(int howMany) {
  // Read raw bytes quickly into rxBuf, then parse in loop (non-blocking)
  uint8_t i = 0;
  while (Wire.available() && i < sizeof(rxBuf) - 1) {
    rxBuf[i++] = (char)Wire.read();
  }
  rxBuf[i] = '\0';
  rxLen = i;
  tickCmdPending = true;
}

static void onRequestEvent() {
  // Send last prepared response (txBuf)
  Wire.write((const uint8_t*)txBuf, (uint8_t)strnlen(txBuf, sizeof(txBuf)));
  // Minimal LED pulse (no delay here)
  if (ledOK) digitalWrite(LED_PIN, HIGH);
}

// ============================================================================
// COMMAND PARSER (runs in loop, safe)
// ============================================================================
static bool parseTickCommand(uint32_t* outTick, uint8_t* outMode, float* outNoise) {
  // Expected: "T,<tick>,<mode>,<noise>"
  // Example:  "T,12,1,0.05"
  char prefix = 0;
  unsigned long t = 0;
  unsigned int m = 0;
  float n = 0.0f;

  int parsed = sscanf(rxBuf, "%c,%lu,%u,%f", &prefix, &t, &m, &n);
  if (parsed < 4 || prefix != 'T') return false;

  *outTick = (uint32_t)t;
  *outMode = (uint8_t)m;
  *outNoise = n;
  return true;
}

// ============================================================================
// TOY DYNAMICS (boundary evolution)
// ============================================================================
static void updateBoundaryState() {
  // Base evolution
  boundaryState = lfsr16_step(boundaryState);

  // Inject noise: flip some bits with probability ~ noise
  float n = clamp01(currentNoise);
  // For pedagogy: approximate bit flips using random threshold
  // Use seedNonce as deterministic-ish PRNG
  seedNonce = seedNonce * 1664525UL + 1013904223UL; // LCG
  uint16_t r = (uint16_t)(seedNonce >> 16);

  // number of bits to flip: 0..3 based on noise
  uint8_t flips = 0;
  if (n > 0.70f) flips = 3;
  else if (n > 0.35f) flips = 2;
  else if (n > 0.10f) flips = 1;

  for (uint8_t i = 0; i < flips; i++) {
    uint8_t bit = (r + i * 7) & 0x0F;
    boundaryState ^= (1u << bit);
  }

  // Observable summary bmask:
  // e.g., parity of 4 groups of 4 bits -> packed as 4-bit mask
  uint16_t s = boundaryState;
  uint8_t g0 = __builtin_parity((unsigned)(s & 0x000Fu));
  uint8_t g1 = __builtin_parity((unsigned)((s >> 4) & 0x000Fu));
  uint8_t g2 = __builtin_parity((unsigned)((s >> 8) & 0x000Fu));
  uint8_t g3 = __builtin_parity((unsigned)((s >> 12) & 0x000Fu));

  bmask = (uint16_t)((g0 << 0) | (g1 << 1) | (g2 << 2) | (g3 << 3));

  // For demonstration: sometimes simulate "loss" when noise high
  if (n > 0.60f && ((r & 0x0030u) == 0x0030u)) {
    lossCount++;
  }
}

// Prepare response string for master
static void buildResponse() {
  // "O,<tick>,<bmask>,<loss>,<noise>,<seed>\n"
  // Keep under 32 bytes
  // Example: O,12,15,0,0.05,12345
  snprintf(txBuf, sizeof(txBuf), "O,%lu,%u,%u,%.2f,%lu\n",
           (unsigned long)currentTick,
           (unsigned)bmask,
           (unsigned)lossCount,
           (double)currentNoise,
           (unsigned long)seedNonce);
}

// ============================================================================
// SETUP / LOOP
// ============================================================================
void setup() {
  Serial.begin(115200);
  Serial.println("\n=== slaveV2 (Toy Boundary Node) ===");
  Serial.print("I2C addr: 0x"); Serial.println(SLAVE_ADDRESS, HEX);

  // LED init (non-fatal)
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  ledOK = true;

  // I2C init
  Wire.begin(SLAVE_ADDRESS);
  Wire.onReceive(onReceiveEvent);
  Wire.onRequest(onRequestEvent);
  i2cOK = true;

  // Seed
  seedNonce = (uint32_t)esp_random(); // works on ESP32; on AVR can fallback
  boundaryState ^= (uint16_t)seedNonce;

  // Initial response
  buildResponse();

  Serial.println("[slaveV2] Ready.");
}

void loop() {
  uint32_t nowMs = millis();

  // Turn off LED shortly after request pulse (non-blocking)
  if (ledOK && digitalRead(LED_PIN) == HIGH) {
    // quick auto-off using time slice: just turn off next loop
    digitalWrite(LED_PIN, LOW);
  }

  // Handle incoming tick command (if any)
  if (tickCmdPending) {
    tickCmdPending = false;

    uint32_t t = 0;
    uint8_t m = 0;
    float n = 0.0f;

    if (parseTickCommand(&t, &m, &n)) {
      currentTick = t;
      currentMode = m;
      currentNoise = n;

      // When a new tick arrives, we can do a burst update (lightweight)
      // Keep it short so it stays responsive.
      updateBoundaryState();
      buildResponse();
    } else {
      Serial.print("[I2C] Bad cmd: ");
      Serial.println(rxBuf);
    }
  }

  // Background evolution (optional) for liveliness even without ticks
  if (nowMs - lastWorkMs >= WORK_STEP_PERIOD_MS) {
    lastWorkMs = nowMs;

    // Only evolve if we have received at least one tick
    if (currentTick > 0) {
      updateBoundaryState();
      buildResponse();
    }
  }

  // Health print
  if (nowMs - lastHealthMs >= HEALTH_PRINT_MS) {
    lastHealthMs = nowMs;
    Serial.print("[slaveV2] tick="); Serial.print(currentTick);
    Serial.print(" bmask="); Serial.print(bmask);
    Serial.print(" loss="); Serial.print(lossCount);
    Serial.print(" noise="); Serial.println(currentNoise, 2);
  }
}
