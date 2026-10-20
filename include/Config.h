/**
 * @file Config.h
 * @brief Versions, feature levels and protocol identity.
 *
 * Feature level follows the target:
 *   - AVR (Uno, Nano, Mega): MINIMAL_FEATURES — compact 10 m track table,
 *     50 Hz physics, no sensor noise, simplified thermal and closed-loop fuel.
 *   - ESP32, ESP8266 and the native build: full model at 100 Hz with sensor
 *     noise, idle and EGO control loops, web dashboard (ESP) and track outline.
 */
#pragma once

#include <stdint.h>

#define FIRMWARE_VERSION "3.0.1"

// Identity of the emulated controller, as a DIY-EFI CORE4 on 202501.6 answers.
#define SPEEDUINO_SIGNATURE "speeduino 202501"      // 'Q'
#define SPEEDUINO_PRODUCT "Speeduino 2025.01.6"      // 'S'
#define SERIAL_PROTOCOL_VERSION "002"                 // 'F'

#define SERIAL_BAUD_RATE 115200

#if defined(ARDUINO_ARCH_AVR)
  #ifndef MINIMAL_FEATURES
    #define MINIMAL_FEATURES
  #endif
#endif

#ifdef MINIMAL_FEATURES
  #define TRACK_COMPACT
  #define SIM_STEP_MS 20
  #define SIM_DETAILED 0
  #define FRAMED_PAYLOAD_MAX 16   // largest request TunerStudio sends is 7 bytes
#else
  #define SIM_STEP_MS 10
  #define SIM_DETAILED 1
  #define FRAMED_PAYLOAD_MAX 256  // page writes carry up to 121 bytes
#endif

#if defined(ENABLE_WEB_INTERFACE) || defined(NATIVE_SIM)
  #define TRACK_OUTLINE
#endif

#ifdef ENABLE_WIFI
  #define WIFI_AP_SSID "SpeeduinoSim"
  #define WIFI_AP_PASSWORD "speeduino123"
  #define WIFI_CONNECT_TIMEOUT_MS 10000
  #define WEB_SERVER_PORT 80
  #define MDNS_HOSTNAME "speeduino-sim"
  #define WIFI_SERIAL_PORT 5000
#endif
