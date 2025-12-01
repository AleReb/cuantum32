/*
 * I2C Master - Opinion Analysis System
 * el i2c en el esp32 s3 super mini usa los pines por defecto 8 y 9 donde 8 es el sda y 9 el scl o sck
 * This master device coordinates multiple I2C slave devices to perform
 * distributed opinion analysis. It aggregates results (favor/contra/neutral)
 * from all slaves and displays the results on an OLED screen with timestamps.
 *  
 * Author: Alejandro Rebolledo (arebolledo@udd.cl)
 * Date: 2025-12-01
 * License: CC BY-NC 4.0
 * 
 * Hardware Requirements:
 * - Arduino board (Uno, Mega, ESP32, etc.)
 * - 1.3" OLED Display (SH1106, I2C address 0x3C)
 * - DS3231 RTC Module (I2C address 0x68)
 * - Multiple slave Arduino devices (addresses 0x10-0x13)
 * - 4.7kΩ pull-up resistors on SDA and SCL lines
 * 
 * Libraries Required:
 * - Wire (built-in)
 * - U8g2 by olikraus
 * - RTClib by Adafruit
 */

#include <Wire.h>
#include "config.h"

// Conditional includes based on configuration
// Conditional includes based on configuration
#include <U8g2lib.h>

#ifdef USE_SH1106_128X64
// Constructor for SH1106 1.3" OLED (128x64, I2C)
U8G2_SH1106_128X64_NONAME_F_HW_I2C display(U8G2_R0, U8X8_PIN_NONE);
#elif defined(USE_SSD1306_128X64)
// Constructor for SSD1306 0.96" OLED (128x64, I2C)
U8G2_SSD1306_128X64_NONAME_F_HW_I2C display(U8G2_R0, U8X8_PIN_NONE);
#endif

#include <RTClib.h>
RTC_DS3231 rtc;

#include <Adafruit_NeoPixel.h>
Adafruit_NeoPixel rgbLed(NUM_RGB_LEDS, RGB_LED_PIN, NEO_GRB + NEO_KHZ800);

#include "FS.h"
#include "SD.h"
#include "SPI.h"
SPIClass spiSD(HSPI);

#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
Adafruit_BME280 bme;  // I2C

// ============================================================================
// GLOBAL VARIABLES
// ============================================================================
float currentTemp;
float currentHum;
float currentPres;

struct OpinionData {
  uint16_t favor;
  uint16_t contra;
  uint16_t neutral;
  bool valid;
};

OpinionData slaveData[NUM_SLAVES];
uint32_t totalFavor = 0;
uint32_t totalContra = 0;
uint32_t totalNeutral = 0;

// ============================================================================
// SETUP
// ============================================================================

void setup() {
  delay(3000);
  Serial.begin(SERIAL_BAUD_RATE);
  Serial.println(F("=== I2C Master - Opinion Analysis System ==="));
  Serial.print(F("Number of slaves: "));
  Serial.println(NUM_SLAVES);

  // Initialize I2C
  if (ENABLE_I2CPINS) {
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  } else {
    Wire.begin();
  }
  // Initialize OLED
  if (ENABLE_OLED) {
    display.begin();
    Serial.println(F("OLED initialized successfully"));
    // Display startup message
    display.clearBuffer();
    display.setFont(u8g2_font_ncenB08_tr);
    display.drawStr(0, 15, "Opinion Analysis");
    display.drawStr(0, 30, "System Ready");
    display.drawStr(0, 45, "I2C Master");
    display.sendBuffer();
    delay(2000);
  }

  // Initialize RTC
  if (ENABLE_RTC) {
    if (!rtc.begin()) {
      Serial.println(F("ERROR: RTC not found!"));
      Serial.println(F("Check wiring to DS3231"));
    } else {
      Serial.println(F("RTC initialized successfully"));
      if (rtc.lostPower()) {
        Serial.println(F("RTC lost power, setting time to compile time"));
        rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
      }
    }
  }


  rgbLed.begin();
  rgbLed.setBrightness(50);  // Set brightness (0-255)
  rgbLed.show();             // Initialize all pixels to 'off'
  Serial.println(F("RGB LED initialized successfully"));

  // Startup animation
  rgbLed.setPixelColor(0, RGB_COLOR_IDLE);
  rgbLed.show();
  delay(500);

  // Initialize SD Card
  if (ENABLE_SD_LOGGING) {
    spiSD.begin(SD_SCLK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);
    if (!SD.begin(SD_CS_PIN, spiSD)) {
      Serial.println(F("ERROR: SD Card initialization failed!"));
    } else {
      Serial.println(F("SD Card initialized successfully"));

      // Create file with header if it doesn't exist
      if (!SD.exists(SD_FILENAME)) {
        File file = SD.open(SD_FILENAME, FILE_WRITE);
        if (file) {
          if (ENABLE_BME280) {
            file.println("Timestamp,Favor,Contra,Neutral,Total,Temp,Humidity");
          } else {
            file.println("Timestamp,Favor,Contra,Neutral,Total");
          }
          file.close();
          Serial.println(F("Created new datalog file"));
        }
      }
    }
  }

  // Initialize BME280
  if (ENABLE_BME280) {
    if (!bme.begin(BME280_ADDRESSD)) {
      Serial.println(F("Could not find a valid BME280 sensor, check wiring!"));
    } else {
      Serial.println(F("BME280 sensor initialized"));
    }
  }

  // Scan I2C bus
  scanI2CBus();
  Serial.println(F("Setup complete. Starting main loop...\n"));
  delay(1000);
}


// ============================================================================
// MAIN LOOP
// ============================================================================

void loop() {
  // Set LED to reading state
  rgbLed.setPixelColor(0, RGB_COLOR_READING);
  rgbLed.show();

  // Request data from all slaves
  bool allValid = requestDataFromAllSlaves();

  // Read BME280 Data (Once per cycle)
  if (ENABLE_BME280) {
    currentTemp = bme.readTemperature();
    currentHum = bme.readHumidity();
    currentPres = bme.readPressure() / 100.0F;
    Serial.println("Temp: " + String(currentTemp) + " C");
  }

  // Update LED based on results
  if (allValid) {
    rgbLed.setPixelColor(0, RGB_COLOR_SUCCESS);
  } else {
    // Check how many slaves responded
    uint8_t validCount = 0;
    for (uint8_t i = 0; i < NUM_SLAVES; i++) {
      if (slaveData[i].valid) validCount++;
    }

    if (validCount == 0) {
      rgbLed.setPixelColor(0, RGB_COLOR_ERROR);  // No slaves responded
    } else {
      rgbLed.setPixelColor(0, RGB_COLOR_WARNING);  // Some slaves responded
    }
  }
  rgbLed.show();

  if (!allValid) {
    Serial.println(F("Warning: Some slaves did not respond"));
  }

  // Aggregate results
  aggregateResults();

  // Log to SD Card
  if (ENABLE_SD_LOGGING) {
    logToSD();
  }

  // Calculate and display percentages
  displayResults();

  // Wait before next cycle
  delay(UPDATE_INTERVAL_MS);

  // Return to idle state
  rgbLed.setPixelColor(0, RGB_COLOR_IDLE);
  rgbLed.show();
}

// ============================================================================
// I2C COMMUNICATION FUNCTIONS
// ============================================================================

bool requestDataFromAllSlaves() {
  bool allValid = true;

  for (uint8_t i = 0; i < NUM_SLAVES; i++) {
    slaveData[i] = requestDataFromSlave(SLAVE_ADDRESSES[i]);

    if (!slaveData[i].valid) {
      allValid = false;
      Serial.print(F("Slave 0x"));
      Serial.print(SLAVE_ADDRESSES[i], HEX);
      Serial.println(F(" - No response"));
    }
  }

  return allValid;
}

OpinionData requestDataFromSlave(uint8_t address) {
  OpinionData data = { 0, 0, 0, false };
  uint8_t attempts = 0;

  while (attempts < MAX_RETRIES && !data.valid) {
    attempts++;

    // Request data from slave
    uint8_t bytesReceived = Wire.requestFrom(address, (uint8_t)32);

    if (bytesReceived > 0) {
      char buf[33];
      uint8_t i = 0;

      // Read into buffer
      while (Wire.available() && i < 32) {
        buf[i] = Wire.read();
        i++;
      }
      buf[i] = '\0';  // Null terminate

      // Parse using sscanf as requested
      int f, c, n;
      int parsed = sscanf(buf, "%d,%d,%d", &f, &c, &n);

      if (parsed >= 3) {
        data.favor = f;
        data.contra = c;
        data.neutral = n;
        data.valid = true;

        // Remove newline for clean printing
        char *newline = strchr(buf, '\n');
        if (newline) *newline = '\0';

        Serial.print(F("Rx 0x"));
        Serial.print(address, HEX);
        Serial.print(F(": "));
        Serial.println(buf);

        // Send acknowledgment "OK"
        Wire.beginTransmission(address);
        Wire.write("OK");
        Wire.endTransmission();
        // Serial.println("Sent acknowledgment: OK");

      } else {
        Serial.print(F("Parse error from 0x"));
        Serial.print(address, HEX);
        Serial.print(F(": "));
        Serial.println(buf);
      }

    } else {
      // Retry after short delay
      if (attempts < MAX_RETRIES) {
        delay(50);
      }
    }
  }

  return data;
}

// ============================================================================
// DATA PROCESSING FUNCTIONS
// ============================================================================

void aggregateResults() {
  totalFavor = 0;
  totalContra = 0;
  totalNeutral = 0;

  for (uint8_t i = 0; i < NUM_SLAVES; i++) {
    if (slaveData[i].valid) {
      totalFavor += slaveData[i].favor;
      totalContra += slaveData[i].contra;
      totalNeutral += slaveData[i].neutral;
    }
  }
}

void displayResults() {
  uint32_t total = totalFavor + totalContra + totalNeutral;

  if (total == 0) {
    Serial.println(F("No data to display (total = 0)"));
    return;
  }

  // Calculate percentages
  float pFavor = (totalFavor * 100.0) / total;
  float pContra = (totalContra * 100.0) / total;
  float pNeutral = (totalNeutral * 100.0) / total;

  // Print to Serial
  Serial.println(F("======== CLUSTER ========"));

  if (ENABLE_RTC) {
    DateTime now = rtc.now();
    Serial.print(F("Time: "));
    Serial.print(now.hour());
    Serial.print(':');
    Serial.print(now.minute());
    Serial.print(':');
    Serial.println(now.second());
  }

  Serial.print(F("A favor  : "));
  Serial.print(pFavor, 1);
  Serial.println(F(" %"));
  Serial.print(F("En contra: "));
  Serial.print(pContra, 1);
  Serial.println(F(" %"));
  Serial.print(F("Dudando  : "));
  Serial.print(pNeutral, 1);
  Serial.println(F(" %"));
  Serial.println(F("=========================="));

  display.clearBuffer();

  // Title
  display.setFont(u8g2_font_ncenB08_tr);
  display.drawStr(0, 10, "OPINION ANALYSIS");
  display.drawLine(0, 12, 128, 12);

  // Timestamp
  if (ENABLE_RTC) {
    DateTime now = rtc.now();
    char timeStr[20];
    sprintf(timeStr, "%02d:%02d:%02d", now.hour(), now.minute(), now.second());
    display.setFont(u8g2_font_6x10_tr);
    display.drawStr(0, 24, timeStr);
  }

  // Slave Status (Right aligned, same line as timestamp)
  display.setFont(u8g2_font_4x6_tr);
  for (uint8_t i = 0; i < NUM_SLAVES && i < 4; i++) {
    int x = 80 + (i * 10);  // Start at x=80
    if (slaveData[i].valid) {
      display.drawStr(x, 24, "OK");
    } else {
      display.drawStr(x, 24, "X");
    }
  }

  // Results
  char buffer[30];
  display.setFont(u8g2_font_6x10_tr);

  // Compressed layout to fit BME280 data
  // y=34: Favor
  sprintf(buffer, "Fav:%.1f%% Con:%.1f%%", pFavor, pContra);
  display.drawStr(0, 34, buffer);

  // y=44: Neutral
  sprintf(buffer, "Neu:%.1f%%", pNeutral);
  display.drawStr(0, 44, buffer);

  if (ENABLE_BME280) {
    // y=54: Temp & Hum
    sprintf(buffer, "T:%.1fC H:%.1f%%", currentTemp, currentHum);
    display.drawStr(0, 54, buffer);

    // y=64: Pressure (optional)
    sprintf(buffer, "P:%.0fhPa", currentPres);
    // display.drawStr(0, 64, buffer);
  }

  display.sendBuffer();
}


// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

void scanI2CBus() {
  Serial.println(F("\nScanning I2C bus..."));
  uint8_t devicesFound = 0;

  for (uint8_t address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    uint8_t error = Wire.endTransmission();

    if (error == 0) {
      Serial.print(F("Device found at 0x"));
      if (address < 16) Serial.print('0');
      Serial.print(address, HEX);

      // Identify known devices
      if (address == 0x3C || address == 0x3D) {
        Serial.print(F(" (OLED Display)"));
      } else if (address == 0x68) {
        Serial.print(F(" (RTC DS3231)"));
      } else if (address >= 0x10 && address <= 0x1F) {
        Serial.print(F(" (Slave Device)"));
      } else if (address == 0x76) {
        Serial.print(F(" (BME 280)"));
      }
      Serial.println();
      devicesFound++;
    }
  }

  if (devicesFound == 0) {
    Serial.println(F("No I2C devices found!"));
    Serial.println(F("Check wiring and pull-up resistors"));
  } else {
    Serial.print(F("Total devices found: "));
    Serial.println(devicesFound);
  }
  Serial.println();
}

void logToSD() {
  File file = SD.open(SD_FILENAME, FILE_APPEND);
  if (!file) {
    Serial.println(F("Failed to open log file for appending"));
    return;
  }

  // Timestamp
  String timestamp = "";
  if (ENABLE_RTC) {
    DateTime now = rtc.now();
    char timeStr[20];
    sprintf(timeStr, "%04d-%02d-%02d %02d:%02d:%02d",
            now.year(), now.month(), now.day(),
            now.hour(), now.minute(), now.second());
    timestamp = String(timeStr);
  } else {
    timestamp = String(millis());
  }

  uint32_t total = totalFavor + totalContra + totalNeutral;

  // Write CSV line: Timestamp,Favor,Contra,Neutral,Total,Temp,Humidity
  file.print(timestamp);
  file.print(",");
  file.print(totalFavor);
  file.print(",");
  file.print(totalContra);
  file.print(",");
  file.print(totalNeutral);
  file.print(",");
  file.print(total);

  if (ENABLE_BME280) {
    file.print(",");
    file.print(currentTemp);
    file.print(",");
    file.print(currentHum);
  }

  file.println();

  file.close();
  Serial.println(F(">> Data saved to SD"));
}
