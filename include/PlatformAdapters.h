/**
 * @file PlatformAdapters.h
 * @brief Arduino implementations of the serial, clock and RNG interfaces.
 */
#pragma once

#include <Arduino.h>
#include "IRandomProvider.h"
#include "ISerialInterface.h"
#include "ITimeProvider.h"

/// Any Arduino Stream: HardwareSerial on AVR/ESP8266, HWCDC/USB on ESP32-S3.
class ArduinoSerialAdapter : public ISerialInterface {
public:
    explicit ArduinoSerialAdapter(Stream& s) : port(s) {}
    void begin(uint32_t) override {}  // the port is opened in setup()
    bool isReady() override { return true; }
    int available() override { return port.available(); }
    int read() override { return port.read(); }
    size_t readBytes(uint8_t* b, size_t n) override { return port.readBytes(b, n); }
    size_t write(uint8_t b) override { return port.write(b); }
    size_t write(const uint8_t* b, size_t n) override { return port.write(b, n); }
    void flush() override { port.flush(); }
    void clear() override {
        while (port.available() > 0) port.read();
    }

private:
    Stream& port;
};

class ArduinoTimeProvider : public ITimeProvider {
public:
    uint32_t millis() override { return ::millis(); }
    uint32_t micros() override { return ::micros(); }
    void delay(uint32_t ms) override { ::delay(ms); }
    void delayMicroseconds(uint32_t us) override { ::delayMicroseconds(us); }
};

class ArduinoRandomProvider : public IRandomProvider {
public:
    void seed(uint32_t s) override { randomSeed(s); }
    int32_t random(int32_t lo, int32_t hi) override { return ::random(lo, hi); }
    int32_t random(int32_t hi) override { return ::random(hi); }
};
