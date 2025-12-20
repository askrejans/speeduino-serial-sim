/**
 * @file Config.h
 * @brief Configuration constants and feature flags for Speeduino Serial Simulator
 * 
 * Platform-specific features:
 * - MINIMAL_FEATURES: Arduino AVR (limited memory)
 * - ADVANCED_SIMULATION: ESP32/ESP8266 (realistic engine model)
 * - ENABLE_WIFI: ESP32/ESP8266 (wireless connectivity)
 * - ENABLE_WEB_INTERFACE: ESP32/ESP8266 (configuration UI)
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>

// ============================================
// Version Information
// ============================================
#define FIRMWARE_VERSION "2.0.0"
#define PROTOCOL_VERSION "0.4"
#define SPEEDUINO_SIGNATURE "speeduino 202310"

// ============================================
// Serial Communication
// ============================================
#define SERIAL_BAUD_RATE 115200
#define SERIAL_TIMEOUT_MS 100
#define SERIAL_BUFFER_SIZE 256

// ============================================
// Engine Simulation Parameters
// ============================================

// RPM ranges (revolution per minute)
#define RPM_MIN 0
#define RPM_IDLE_MIN 700
#define RPM_IDLE_MAX 900
#define RPM_CRUISE 2500
#define RPM_HIGH_START 5000
#define RPM_MAX 7000
#define RPM_REDLINE 6800

// Temperature ranges (Celsius * 10)
#define TEMP_AMBIENT 200        // 20°C
#define TEMP_ENGINE_COLD 400    // 40°C
#define TEMP_ENGINE_WARM 800    // 80°C
#define TEMP_ENGINE_HOT 950     // 95°C
#define TEMP_ENGINE_MAX 1100    // 110°C
#define TEMP_IAT_COLD 100       // 10°C
#define TEMP_IAT_WARM 400       // 40°C

// Pressure (kPa)
#define MAP_ATMOSPHERIC 100
#define MAP_IDLE 35
#define MAP_CRUISE 60
#define MAP_WOT 95
#define BARO_SEALEVEL 100

// Voltage (Volts * 10)
#define VOLTAGE_MIN 110         // 11.0V
#define VOLTAGE_NORMAL 140      // 14.0V
#define VOLTAGE_MAX 150         // 15.0V

// Air-Fuel Ratio (AFR * 10)
#define AFR_STOICH 147          // 14.7:1
#define AFR_RICH 130            // 13.0:1
#define AFR_LEAN 160            // 16.0:1
#define AFR_WOT 125             // 12.5:1 (power enrichment)

// Throttle Position (0-100%)
#define TPS_IDLE 2
#define TPS_CRUISE 20
#define TPS_HALF 50
#define TPS_WOT 100

// Ignition timing (degrees BTDC)
#define TIMING_IDLE 15
#define TIMING_CRUISE 25
#define TIMING_WOT 30
#define TIMING_MAX 35

// Pulse width (milliseconds * 10)
#define PW_MIN 10               // 1.0ms
#define PW_IDLE 20              // 2.0ms
#define PW_CRUISE 35            // 3.5ms
#define PW_WOT 80               // 8.0ms
#define PW_MAX 255              // 25.5ms

// ============================================
// Simulation Timing
// ============================================
#define UPDATE_INTERVAL_MS 50          // 20Hz update rate
#define STATE_TRANSITION_MS 5000       // 5 seconds between state changes
#define WARMUP_TIME_MS 30000           // 30 seconds to warm up engine

#ifdef MINIMAL_FEATURES
  #define SENSOR_NOISE_ENABLED 0
  #define TRANSIENT_SIMULATION 0
  #define REALISTIC_CORRELATION 0
#else
  #define SENSOR_NOISE_ENABLED 1
  #define TRANSIENT_SIMULATION 1
  #define REALISTIC_CORRELATION 1
#endif

// ============================================
// WiFi Configuration (ESP32/ESP8266 only)
// ============================================
#ifdef ENABLE_WIFI
  #define WIFI_SSID "SpeeduinoSim"
  #define WIFI_PASSWORD "speeduino123"
  #define WIFI_TIMEOUT_MS 10000
  #define WEB_SERVER_PORT 80
  #define MDNS_HOSTNAME "speeduino-sim"
#endif

// ============================================
// Debugging
// ============================================
#ifdef DEBUG
  #define DEBUG_PRINT(x) Serial.print(x)
  #define DEBUG_PRINTLN(x) Serial.println(x)
#else
  #define DEBUG_PRINT(x)
  #define DEBUG_PRINTLN(x)
#endif

// ============================================
// Physical Constants (for realistic simulation)
// ============================================
#define ENGINE_DISPLACEMENT 2000    // 2.0L in cc
#define NUM_CYLINDERS 4             // I4 engine
#define FUEL_DENSITY 737            // gasoline g/L
#define AIR_DENSITY 1225            // g/m³ at sea level

#endif // CONFIG_H
