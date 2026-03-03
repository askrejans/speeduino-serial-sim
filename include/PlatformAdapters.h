/**
 * @file PlatformAdapters.h
 * @brief Platform-specific implementations of hardware abstraction interfaces
 * 
 * Provides concrete implementations for Arduino, ESP32, and ESP8266.
 */

#ifndef PLATFORM_ADAPTERS_H
#define PLATFORM_ADAPTERS_H

#include "ISerialInterface.h"
#include "ITimeProvider.h"
#include "IRandomProvider.h"

#if defined(ENABLE_WIFI) && defined(ENABLE_WIFI_SERIAL)
  #include "WiFiSerialAdapter.h"
#endif

#if defined(ARDUINO)
  #include <Arduino.h>
#endif

// ============================================
// Arduino Serial Adapter
// ============================================

class ArduinoSerialAdapter : public ISerialInterface {
private:
    Stream* serial;
    uint32_t baudRate;
    
public:
    // Use Stream base class to support both HardwareSerial and HWCDC (ESP32-S3 USB)
    explicit ArduinoSerialAdapter(Stream* serialPort = &Serial) 
        : serial(serialPort), baudRate(0) {}
    
    void begin(uint32_t baud) override {
        baudRate = baud;
        
        // Begin serial based on platform type
        #if defined(ESP32)
            // On ESP32-S3 with USB CDC, Serial is HWCDC which has begin()
            // On other ESP32s, Serial is HardwareSerial
            #if defined(ARDUINO_USB_CDC_ON_BOOT) && ARDUINO_USB_CDC_ON_BOOT
                // USB CDC mode - serial is already initialized
                Serial.begin(baud);
            #else
                // Standard UART mode
                if (serial == &Serial) {
                    Serial.begin(baud);
                } else {
                    ((HardwareSerial*)serial)->begin(baud);
                }
            #endif
        #else
            // Standard Arduino - assume HardwareSerial
            ((HardwareSerial*)serial)->begin(baud);
        #endif
        
        // Short delay to allow serial to stabilize
        delay(100);
    }
    
    bool isReady() override {
        return serial != nullptr;
    }
    
    int available() override {
        return serial->available();
    }
    
    int read() override {
        return serial->read();
    }
    
    size_t readBytes(uint8_t* buffer, size_t length) override {
        return serial->readBytes(buffer, length);
    }
    
    size_t write(uint8_t byte) override {
        return serial->write(byte);
    }
    
    size_t write(const uint8_t* buffer, size_t length) override {
        return serial->write(buffer, length);
    }
    
    void flush() override {
        serial->flush();
    }
    
    void clear() override {
        while (serial->available() > 0) {
            serial->read();
        }
    }
};

// ============================================
// Arduino Time Provider
// ============================================

class ArduinoTimeProvider : public ITimeProvider {
public:
    uint32_t millis() override {
        return ::millis();
    }
    
    uint32_t micros() override {
        return ::micros();
    }
    
    void delay(uint32_t ms) override {
        ::delay(ms);
    }
    
    void delayMicroseconds(uint32_t us) override {
        ::delayMicroseconds(us);
    }
};

// ============================================
// Arduino Random Provider
// ============================================

class ArduinoRandomProvider : public IRandomProvider {
public:
    void seed(uint32_t seed) override {
        randomSeed(seed);
    }
    
    int32_t random(int32_t min, int32_t max) override {
        return ::random(min, max);
    }
    
    int32_t random(int32_t max) override {
        return ::random(max);
    }
};

// ============================================
// Factory Functions
// ============================================

/**
 * @brief Create platform-appropriate serial interface
 * @return Pointer to serial interface (caller owns memory)
 * 
 * When ENABLE_WIFI_SERIAL is defined, returns a WiFiSerialAdapter
 * that listens on WIFI_SERIAL_PORT for TCP connections.
 * Otherwise returns the default hardware serial adapter.
 */
inline ISerialInterface* createSerialInterface() {
#if defined(ENABLE_WIFI) && defined(ENABLE_WIFI_SERIAL)
    return new WiFiSerialAdapter(WIFI_SERIAL_PORT);
#else
    return new ArduinoSerialAdapter();
#endif
}

/**
 * @brief Create platform-appropriate time provider
 * @return Pointer to time provider (caller owns memory)
 */
inline ITimeProvider* createTimeProvider() {
    return new ArduinoTimeProvider();
}

/**
 * @brief Create platform-appropriate random provider
 * @return Pointer to random provider (caller owns memory)
 */
inline IRandomProvider* createRandomProvider() {
    return new ArduinoRandomProvider();
}

#endif // PLATFORM_ADAPTERS_H
