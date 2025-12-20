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

#if defined(ARDUINO)
  #include <Arduino.h>
#endif

// ============================================
// Arduino Serial Adapter
// ============================================

class ArduinoSerialAdapter : public ISerialInterface {
private:
    HardwareSerial* serial;
    
public:
    explicit ArduinoSerialAdapter(HardwareSerial* serialPort = &Serial) 
        : serial(serialPort) {}
    
    void begin(uint32_t baudRate) override {
        serial->begin(baudRate);
        // Wait for serial port to be ready
        while (!serial) {
            ; // Wait for serial port to connect
        }
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
 */
inline ISerialInterface* createSerialInterface() {
    return new ArduinoSerialAdapter();
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
