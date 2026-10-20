/**
 * @file NativeAdapters.h
 * @brief Desktop (macOS/Linux) adapters: clock, RNG, and the serial line as a
 *        TCP server or a pseudo-terminal.
 */
#pragma once

#if defined(NATIVE_SIM) && !defined(ARDUINO)

#include <chrono>
#include <random>
#include <thread>
#include "IRandomProvider.h"
#include "ISerialInterface.h"
#include "ITimeProvider.h"

/// Milliseconds since start, optionally running faster than real time.
class NativeTime : public ITimeProvider {
public:
    explicit NativeTime(double speed = 1.0) : speed(speed), start(std::chrono::steady_clock::now()) {}
    uint32_t millis() override { return (uint32_t)(elapsedUs() / 1000); }
    uint32_t micros() override { return (uint32_t)elapsedUs(); }
    void delay(uint32_t ms) override { std::this_thread::sleep_for(std::chrono::microseconds((long)(ms * 1000 / speed))); }
    void delayMicroseconds(uint32_t us) override { std::this_thread::sleep_for(std::chrono::microseconds((long)(us / speed))); }

private:
    double speed;
    std::chrono::steady_clock::time_point start;
    double elapsedUs() const {
        return std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - start).count() * speed;
    }
};

class NativeRandom : public IRandomProvider {
public:
    void seed(uint32_t s) override { rng.seed(s); }
    int32_t random(int32_t lo, int32_t hi) override {
        if (hi <= lo) return lo;
        return std::uniform_int_distribution<int32_t>(lo, hi - 1)(rng);
    }
    int32_t random(int32_t hi) override { return random(0, hi); }

private:
    std::mt19937 rng{86};
};

/// One client at a time on a TCP port (TunerStudio "TCP/IP", ecu-to-mqtt
/// connection_type = "tcp"). Bytes are passed through untouched.
class TcpSerial : public ISerialInterface {
public:
    TcpSerial(const char* bind, uint16_t port);
    ~TcpSerial() override;
    bool listening() const { return server >= 0; }
    bool connected() const { return client >= 0; }
    void begin(uint32_t) override {}
    bool isReady() override;
    int available() override;
    int read() override;
    size_t readBytes(uint8_t* b, size_t n) override;
    size_t write(uint8_t b) override { return write(&b, 1); }
    size_t write(const uint8_t* b, size_t n) override;
    void flush() override {}
    void clear() override;

private:
    int server = -1;
    int client = -1;
    void accept();
    void drop();
};

/// A pseudo-terminal: serial tools open the printed device path
/// (e.g. /dev/ttys004) exactly like a USB adapter.
class PtySerial : public ISerialInterface {
public:
    PtySerial();
    ~PtySerial() override;
    const char* path() const { return name; }
    bool ok() const { return master >= 0; }
    void begin(uint32_t) override {}
    bool isReady() override { return master >= 0; }
    int available() override;
    int read() override;
    size_t readBytes(uint8_t* b, size_t n) override;
    size_t write(uint8_t b) override { return write(&b, 1); }
    size_t write(const uint8_t* b, size_t n) override;
    void flush() override {}
    void clear() override;

private:
    int master = -1;
    int slave = -1;  // kept open so the device survives client reconnects
    char name[128] = {};
};

#endif
