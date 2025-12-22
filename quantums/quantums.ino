/*
 * I2C Slave - Opinion Analysis Worker
 * 
 * This slave device performs opinion analysis simulation and responds to
 * I2C requests from the master with aggregated opinion data (favor/contra/neutral).
 * 
 * Author: Alejandro Rebolledo (arebolledo@udd.cl)
 * Date: 2025-12-01
 * License: CC BY-NC 4.0
 * 
 * Hardware Requirements:
 * - Arduino board (Uno, Nano, ESP32, etc.)
 * - Connection to I2C bus (SDA/SCL with 4.7kΩ pull-ups)
 * 
 * Configuration:
 * - Set SLAVE_ADDRESS to a unique value for each slave (0x10, 0x11, 0x12, 0x13, etc.)
 * - Avoid addresses: 0x3C/0x3D (OLED), 0x68 (RTC)
 * 
 * Libraries Required:
 * - Wire (built-in)
 */

#include <Wire.h>

// ============================================================================
// CONFIGURATION - CHANGE THIS FOR EACH SLAVE!
// ============================================================================

#define SLAVE_ADDRESS     0x10    // CHANGE THIS: 0x10, 0x11, 0x12, 0x13, etc.
#define N_SAMPLES         256     // Number of samples to process per cycle

// ============================================================================
// GLOBAL VARIABLES
// ============================================================================

// Opinion counters
volatile uint16_t favorCount = 0;
volatile uint16_t contraCount = 0;
volatile uint16_t neutralCount = 0;

// State machine
uint32_t currentStep = 0;
bool dataReady = false;

// LED pin for status indication (optional)
#define LED_PIN 7
bool ledState = false;

// ============================================================================
// SETUP
// ============================================================================

void setup() {

  Serial.begin(115200);
  Serial.println(F("=== I2C Slave - Opinion Analysis Worker ==="));
  Serial.print(F("I2C Address: 0x"));
  Serial.println(SLAVE_ADDRESS, HEX);
  
  // Initialize LED
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  
  // Initialize I2C as slave
  Wire.begin(SLAVE_ADDRESS);
  Wire.onRequest(requestEvent);  // Register callback for I2C requests
  
  // Initialize random seed for simulation
  randomSeed(analogRead(A0));
  
  // Perform initial analysis
  performAnalysis();
  
  Serial.println(F("Slave ready and waiting for master requests...\n"));
}

// ============================================================================
// MAIN LOOP
// ============================================================================

void loop() {
  // Continuously perform analysis to simulate real-time data
  // In a real application, this would be replaced with actual sensor readings
  // or data processing
  
  static unsigned long lastUpdate = 0;
  static unsigned long ledOffTime = 0;
  unsigned long currentTime = millis();
  
  // Turn off LED after transmission (non-blocking)
  if (digitalRead(LED_PIN) == HIGH && (currentTime - ledOffTime > 10)) {
    digitalWrite(LED_PIN, LOW);
  }
  
  // Update analysis every 500ms
  if (currentTime - lastUpdate >= 500) {
    lastUpdate = currentTime;
    performAnalysis();
    
    // Blink LED to show activity
    ledState = !ledState;
    digitalWrite(LED_PIN, ledState);
    ledOffTime = currentTime;
    
    // Print status every 10 steps
    if (currentStep % 10 == 0) {
      printStatus();
    }
  }
}

// ============================================================================
// I2C CALLBACK FUNCTION
// ============================================================================

void requestEvent() {
  // This function is called when the master requests data
  // Send data as CSV string: "favor,contra,neutral\n"
  // This matches the format expected by the master
  
  String payload = String(favorCount) + "," + String(contraCount) + "," + String(neutralCount) + "\n";
  Wire.write(payload.c_str());
  
  // Log the transmission
  Serial.print(F("Data sent to master: "));
  Serial.print(favorCount);
  Serial.print(F(", "));
  Serial.print(contraCount);
  Serial.print(F(", "));
  Serial.println(neutralCount);
  
  // Blink LED rapidly to indicate transmission (non-blocking)
  digitalWrite(LED_PIN, HIGH);
  // Note: Cannot use delay() in ISR, LED will be turned off in main loop
}

void receiveEvent(int howMany) {
  String msg = "";
  while (Wire.available()) {
    msg += (char)Wire.read();
  }
  msg.trim();
  
  if (msg.equalsIgnoreCase("OK")) {
    // Master acknowledged receipt
    // You could add logic here if needed, e.g., reset counters or flag success
    // For now, we just log it (careful with Serial in ISR, keep it short)
    // Serial.println("ACK received"); 
  }
}

// ============================================================================
// ANALYSIS FUNCTIONS
// ============================================================================

void performAnalysis() {
  // Simulate opinion analysis
  // In a real application, this would process actual data
  
  // Reset counters
  favorCount = 0;
  contraCount = 0;
  neutralCount = 0;
  
  // Simulate N_SAMPLES opinions with random distribution
  for (uint16_t i = 0; i < N_SAMPLES; i++) {
    int opinion = simulateOpinion();
    
    switch (opinion) {
      case 0:
        favorCount++;
        break;
      case 1:
        contraCount++;
        break;
      case 2:
        neutralCount++;
        break;
    }
  }
  
  currentStep++;
  dataReady = true;
}

int simulateOpinion() {
  // Simulate opinion with weighted probabilities
  // This creates more realistic-looking data
  
  int randValue = random(100);
  
  // Example distribution (adjust as needed):
  // 40% favor, 35% contra, 25% neutral
  if (randValue < 40) {
    return 0;  // Favor
  } else if (randValue < 75) {
    return 1;  // Contra
  } else {
    return 2;  // Neutral
  }
}

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

void printStatus() {
  uint16_t total = favorCount + contraCount + neutralCount;
  
  if (total == 0) return;
  
  float pFavor = (favorCount * 100.0) / total;
  float pContra = (contraCount * 100.0) / total;
  float pNeutral = (neutralCount * 100.0) / total;
  
  Serial.print(F("Step "));
  Serial.print(currentStep);
  Serial.print(F(" | Favor: "));
  Serial.print(pFavor, 1);
  Serial.print(F("% | Contra: "));
  Serial.print(pContra, 1);
  Serial.print(F("% | Neutral: "));
  Serial.print(pNeutral, 1);
  Serial.println(F("%"));
}

// ============================================================================
// ADVANCED FEATURES (Optional)
// ============================================================================

/*
 * For real-world applications, you might want to add:
 * 
 * 1. Sensor Integration:
 *    - Read from actual sensors (temperature, light, etc.)
 *    - Process sensor data to generate opinions
 * 
 * 2. Data Validation:
 *    - Add checksums to I2C transmissions
 *    - Implement error detection
 * 
 * 3. Configuration via I2C:
 *    - Allow master to configure slave parameters
 *    - Implement I2C receive callback (Wire.onReceive)
 * 
 * 4. EEPROM Storage:
 *    - Store configuration in EEPROM
 *    - Persist data across power cycles
 * 
 * 5. Watchdog Timer:
 *    - Implement watchdog for reliability
 *    - Auto-reset on hang
 */
