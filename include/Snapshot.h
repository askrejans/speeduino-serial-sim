/**
 * @file Snapshot.h
 * @brief JSON documents for the web dashboard and HTTP API.
 *
 * Built in the main loop into caller-owned buffers, so web handlers running
 * on another task never touch the simulator.
 */
#pragma once

#include <stddef.h>
#include <stdint.h>
#include "EngineSimulator.h"
#include "SpeeduinoProtocol.h"

class JsonWriter {
public:
    JsonWriter(char* buf, size_t cap) : buf(buf), cap(cap) { buf[0] = 0; }
    JsonWriter& open(const char* key = nullptr);
    JsonWriter& close();
    JsonWriter& openArray(const char* key = nullptr);
    JsonWriter& closeArray();
    JsonWriter& num(const char* key, double v, uint8_t decimals = 1);
    JsonWriter& integer(const char* key, long v);
    JsonWriter& boolean(const char* key, bool v);
    JsonWriter& str(const char* key, const char* v);
    size_t length() const { return len; }
    bool overflow() const { return over; }

private:
    char* buf;
    size_t cap;
    size_t len = 0;
    bool first = true;
    bool over = false;
    void raw(const char* s);
    void key(const char* k);
    void escaped(const char* s);
};

struct DeviceInfo {
    const char* platform = "";
    uint32_t freeHeap = 0;
    const char* wifiMode = "";
    const char* ip = "";
    const char* ssid = "";
    int rssi = 0;
    uint8_t streamClients = 0;
    bool serialClient = false;
    const char* serialTransport = "";
    uint32_t nowMs = 0;
};

/// ~10 Hz stream document: phase, lap timing, car and main ECU values.
size_t liveJson(char* buf, size_t cap, const EngineSimulator& sim);
/// Raw 130-byte packet (decoded in the browser by byte offset).
size_t packetJson(char* buf, size_t cap, const EngineSimulator& sim);
/// Device, protocol counters and settings.
size_t statusJson(char* buf, size_t cap, EngineSimulator& sim, const ProtocolStats& stats, const DeviceInfo& dev);
#ifdef TRACK_OUTLINE
/// Track outline for maps (metres east/north of the start line).
size_t trackJson(char* buf, size_t cap);
#endif
