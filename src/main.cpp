/**
 * @file main.cpp
 * @brief Main entry point for Speeduino Serial Simulator
 * 
 * Cross-platform ECU simulator supporting:
 * - Arduino AVR (minimal features)
 * - ESP32 (full features + WiFi + web interface)
 * - ESP8266 (full features + WiFi + web interface)
 * 
 * Features:
 * - Realistic I4 engine simulation
 * - Speeduino protocol compatibility (commands A, Q, V, S, n)
 * - Web interface for monitoring and control (ESP only)
 * - Platform-specific optimizations
 */

#ifndef UNIT_TEST  // Exclude entire file during test builds

#include <Arduino.h>
#include "Config.h"
#include "EngineStatus.h"
#include "EngineSimulator.h"
#include "SpeeduinoProtocol.h"
#include "PlatformAdapters.h"

#ifdef ENABLE_WEB_INTERFACE
  #include "WebInterface.h"
#endif

// Global objects (with dependency injection for testability)
ISerialInterface* serialInterface = nullptr;
ITimeProvider* timeProvider = nullptr;
IRandomProvider* randomProvider = nullptr;
EngineSimulator* engineSimulator = nullptr;
SpeeduinoProtocol* protocol = nullptr;

#ifdef ENABLE_WEB_INTERFACE
  WebInterface* webInterface = nullptr;
#endif

// Status LED pin (if available)
#ifdef LED_BUILTIN
  #define STATUS_LED LED_BUILTIN
#else
  #define STATUS_LED -1
#endif

/**
 * @brief Setup function - called once at startup
 */
void setup() {
    // Initialize status LED
    #if STATUS_LED >= 0
        pinMode(STATUS_LED, OUTPUT);
        digitalWrite(STATUS_LED, HIGH);
    #endif
    
    // Create platform-specific adapters
    serialInterface = createSerialInterface();
    timeProvider = createTimeProvider();
    randomProvider = createRandomProvider();
    
    // Initialize hardware serial for debugging (always needed for ESP32/ESP8266)
    #if defined(ESP32) || defined(ESP8266)
        Serial.begin(SERIAL_BAUD_RATE);
        // Wait for USB CDC to be ready on ESP32-S3
        #ifdef ESP32
            delay(1000);
        #else
            delay(500);
        #endif
        
        // Early boot diagnostic
        Serial.println("\n\n*** BOOT START ***");
        #ifdef ESP32
            Serial.print("Reset reason: ");
            Serial.println(esp_reset_reason());
        #endif
    #endif
    
    // Initialize serial communication interface
    serialInterface->begin(SERIAL_BAUD_RATE);
    
    // Wait for serial to be ready (important for USB serial)
    #ifdef ARDUINO_AVR
        delay(100);  // Short delay for AVR
    #endif
    
    // Print startup banner
    Serial.println("\n\n================================");
    Serial.println("Speeduino Serial Simulator");
    Serial.println("Version: " FIRMWARE_VERSION);
    Serial.println("Protocol: " PROTOCOL_VERSION);
    
    #ifdef ARDUINO_AVR
        Serial.println("Platform: Arduino AVR (minimal)");
    #elif defined(ESP32)
        Serial.println("Platform: ESP32 (full features)");
    #elif defined(ESP8266)
        Serial.println("Platform: ESP8266 (full features)");
    #else
        Serial.println("Platform: Unknown");
    #endif
    
    Serial.println("================================\n");
    
    // Create engine simulator
    Serial.println("Initializing engine simulator...");
    engineSimulator = new EngineSimulator(timeProvider, randomProvider);
    engineSimulator->initialize();
    Serial.println("✓ Engine simulator ready");
    
    // Create protocol handler
    Serial.println("Initializing protocol handler...");
    protocol = new SpeeduinoProtocol(serialInterface, engineSimulator);
    protocol->begin();
    Serial.println("✓ Protocol handler ready");
    
    #ifdef ENABLE_WEB_INTERFACE
        // Initialize web interface (ESP32/ESP8266 only)
        Serial.println("Initializing web interface...");
        webInterface = new WebInterface(engineSimulator, protocol);
        
        // Auto-detect mode: tries station if configured, falls back to AP
        if (webInterface->begin()) {
            Serial.println("✓ Web interface ready");
            Serial.print("WiFi Mode: ");
            Serial.println(webInterface->isAccessPointMode() ? "Access Point" : "Station");
            Serial.print("Access at: http://");
            Serial.println(webInterface->getIP());
            Serial.println("Configure WiFi at: http://" + webInterface->getIP().toString() + "/wifi");
        } else {
            Serial.println("✗ Web interface failed");
        }
    #endif
    
    #if defined(ENABLE_WIFI) && defined(ENABLE_WIFI_SERIAL)
        Serial.print("WiFi serial listening on TCP port ");
        Serial.println(WIFI_SERIAL_PORT);
    #endif

    Serial.println("\nSimulator started!");
    Serial.println("Waiting for commands on serial port...\n");
    
    // LED off to indicate ready
    #if STATUS_LED >= 0
        digitalWrite(STATUS_LED, LOW);
    #endif
}

/**
 * @brief Main loop - called repeatedly
 */
void loop() {
    // Blink LED on activity
    static uint32_t lastActivityTime = 0;
    static bool ledState = false;
    
    // Update engine simulation
    if (engineSimulator->update()) {
        // State changed, print status on Arduino (not on ESP to avoid spam)
        #ifdef MINIMAL_FEATURES
            static uint32_t lastPrint = 0;
            if (millis() - lastPrint > 5000) {  // Every 5 seconds
                const EngineStatus& status = engineSimulator->getStatus();
                Serial.print("Mode: ");
                Serial.print((int)engineSimulator->getMode());
                Serial.print(" | RPM: ");
                Serial.print(status.getRPM());
                Serial.print(" | CLT: ");
                Serial.print(status.getCoolantTemp());
                Serial.print("°C | MAP: ");
                Serial.print(status.getMAP());
                Serial.println(" kPa");
                lastPrint = millis();
            }
        #endif
    }
    
    // Process serial commands
    if (protocol->processCommands()) {
        lastActivityTime = millis();
        ledState = true;
        
        #if STATUS_LED >= 0
            digitalWrite(STATUS_LED, HIGH);
        #endif
    }
    
    // Turn off LED after activity
    if (ledState && (millis() - lastActivityTime > 50)) {
        ledState = false;
        #if STATUS_LED >= 0
            digitalWrite(STATUS_LED, LOW);
        #endif
    }
    
    #ifdef ENABLE_WEB_INTERFACE
        // Update web interface
        webInterface->update();
    #endif
    
    // Yield to other tasks (important for ESP8266)
    #ifdef ESP8266
        yield();
    #endif
}

#endif // UNIT_TEST
