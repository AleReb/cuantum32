/*
 * Configuration Header for I2C Multi-Slave Opinion Analysis System
 * 
 * Author: Alejandro Rebolledo (arebolledo@udd.cl)
 * Date: 2025-12-01
 * License: CC BY-NC 4.0
 * 
 * This file centralizes all configuration parameters for the master and slave devices.
 */

#ifndef CONFIG_H
#define CONFIG_H

// ============================================================================
// I2C CONFIGURATION
// ============================================================================

// I2C Slave Addresses (avoid 0x3C/0x3D for OLED, 0x68 for RTC DS3231)
#define SLAVE_ADDR_1    0x10  // 16
#define SLAVE_ADDR_2    0x11  // 17
#define SLAVE_ADDR_3    0x12  // 18
#define SLAVE_ADDR_4    0x13  // 19

// Array of active slave addresses
const uint8_t SLAVE_ADDRESSES[] = {
  SLAVE_ADDR_1,
  SLAVE_ADDR_2,
  SLAVE_ADDR_3,
  SLAVE_ADDR_4
};

// Number of active slaves
#define NUM_SLAVES (sizeof(SLAVE_ADDRESSES) / sizeof(SLAVE_ADDRESSES[0]))

// I2C Communication Settings
#define I2C_CLOCK_SPEED   100000  // 100 kHz (standard mode), can use 400000 for fast mode
#define I2C_TIMEOUT_MS    100     // Timeout for I2C requests in milliseconds
#define MAX_RETRIES       3       // Maximum retry attempts for failed I2C communication

// ============================================================================
// OLED DISPLAY CONFIGURATION (U8g2 Library)
// ============================================================================

#define ENABLE_OLED       true    // Set to false to disable OLED functionality
#define OLED_ADDRESS      0x3C    // Common address for SSD1306/SH1106 (or 0x3D)

// U8g2 Constructor Selection
// Uncomment ONE of the following based on your display:

// For 1.3" OLED SH1106 128x64 I2C
#define USE_SH1106_128X64

// For 0.96" OLED SSD1306 128x64 I2C
// #define USE_SSD1306_128X64

// ============================================================================
// RTC CONFIGURATION
// ============================================================================

#define ENABLE_RTC        true    // Set to false to disable RTC functionality
#define RTC_ADDRESS       0x68    // DS3231 RTC address (fixed)

// ============================================================================
// TIMING CONFIGURATION
// ============================================================================

#define UPDATE_INTERVAL_MS  1500  // Delay between polling cycles (milliseconds)
#define SERIAL_BAUD_RATE    115200


// ============================================================================
// Buttons config
// ============================================================================

#define BTNA       1    // just extra UX
#define BTNB       2    // just extra UX
// ============================================================================
// DATA PROTOCOL
// ============================================================================

#define DATA_PACKET_SIZE  6       // 3 x 16-bit integers (favor, contra, neutral)
#define SAMPLES_PER_SLAVE 256     // Number of samples each slave processes

// ============================================================================
// RGB LED CONFIGURATION (WS2812B/NeoPixel)
// ============================================================================

#define ENABLE_RGB_LED    true    // Set to false to disable RGB LED
#define RGB_LED_PIN       48      // Pin for addressable RGB LED
#define NUM_RGB_LEDS      1       // Number of LEDs in strip (1 for single LED)

// RGB LED Colors (R, G, B values 0-255)
#define RGB_COLOR_OFF       0, 0, 0
#define RGB_COLOR_IDLE      0, 0, 50      // Blue - waiting
#define RGB_COLOR_READING   255, 255, 0   // Yellow - reading from slaves
#define RGB_COLOR_SUCCESS   0, 255, 0     // Green - all slaves OK
#define RGB_COLOR_WARNING   255, 128, 0   // Orange - some slaves failed
#define RGB_COLOR_ERROR     255, 0, 0     // Red - all slaves failed

// ============================================================================
// SD CARD DATALOGGER CONFIGURATION
// ============================================================================

#define ENABLE_SD_LOGGING true    // Set to false to disable SD logging
#define SD_CS_PIN         7       // Chip Select pin
#define SD_MOSI_PIN       6       // MOSI pin
#define SD_MISO_PIN       5       // MISO pin
#define SD_SCLK_PIN       4       // SCLK pin
#define SD_FILENAME       "/datalog.csv"

// ============================================================================
// BME280 SENSOR CONFIGURATION
// ============================================================================

#define ENABLE_BME280     true    // Set to false to disable BME280
#define BME280_ADDRESSD    0x76    // I2C Address (0x76 or 0x77)
#define SEALEVELPRESSURE_HPA (1013.25)

// ============================================================================
// PIN DEFINITIONS (Optional - for boards with multiple I2C buses)
// ============================================================================
#define ENABLE_I2CPINS      false    // Set to true to change pins i2c functionality
// Most Arduino boards use default I2C pins:
// - Arduino Uno/Nano: SDA = A4, SCL = A5
// - Arduino Mega: SDA = 20, SCL = 21
// - ESP32: SDA = 21, SCL = 22 (configurable)
// - ESP8266: SDA = 4 (D2), SCL = 5 (D1)
// -ESP32S3 Super mini: SDA = 8, SCL = 9 (configurable)
// Uncomment and modify if using custom I2C pins (ESP32/ESP8266)
 #define I2C_SDA_PIN 21
 #define I2C_SCL_PIN 22

#endif // CONFIG_H
