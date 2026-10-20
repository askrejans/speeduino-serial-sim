/**
 * @file Snapshot.cpp
 * @brief Hand-written JSON (no allocation, no library) for the web side.
 */
#include "Snapshot.h"
#include <stdio.h>
#include <string.h>
#include "Track.h"

void JsonWriter::raw(const char* s) {
    size_t n = strlen(s);
    if (len + n + 1 > cap) {
        over = true;
        return;
    }
    memcpy(buf + len, s, n + 1);
    len += n;
}

void JsonWriter::escaped(const char* s) {
    raw("\"");
    char one[7];
    for (; *s; s++) {
        unsigned char c = (unsigned char)*s;
        if (c == '"' || c == '\\') {
            one[0] = '\\';
            one[1] = (char)c;
            one[2] = 0;
        } else if (c < 0x20) {
            snprintf(one, sizeof(one), "\\u%04x", c);
        } else {
            one[0] = (char)c;
            one[1] = 0;
        }
        raw(one);
    }
    raw("\"");
}

void JsonWriter::key(const char* k) {
    if (!first) raw(",");
    first = false;
    if (k) {
        escaped(k);
        raw(":");
    }
}

JsonWriter& JsonWriter::open(const char* k) {
    key(k);
    raw("{");
    first = true;
    return *this;
}
JsonWriter& JsonWriter::close() {
    raw("}");
    first = false;
    return *this;
}
JsonWriter& JsonWriter::openArray(const char* k) {
    key(k);
    raw("[");
    first = true;
    return *this;
}
JsonWriter& JsonWriter::closeArray() {
    raw("]");
    first = false;
    return *this;
}
JsonWriter& JsonWriter::num(const char* k, double v, uint8_t decimals) {
    key(k);
    char t[24];
    if (v != v) snprintf(t, sizeof(t), "null");
    else snprintf(t, sizeof(t), "%.*f", decimals, v);
    raw(t);
    return *this;
}
JsonWriter& JsonWriter::integer(const char* k, long v) {
    key(k);
    char t[16];
    snprintf(t, sizeof(t), "%ld", v);
    raw(t);
    return *this;
}
JsonWriter& JsonWriter::boolean(const char* k, bool v) {
    key(k);
    raw(v ? "true" : "false");
    return *this;
}
JsonWriter& JsonWriter::str(const char* k, const char* v) {
    key(k);
    escaped(v ? v : "");
    return *this;
}

size_t liveJson(char* buf, size_t cap, const EngineSimulator& sim) {
    const EngineStatus& s = sim.getStatus();
    const Telemetry& t = sim.telemetry();
    JsonWriter j(buf, cap);
    j.open();
    j.integer("uptime", (long)sim.uptimeS());
    j.str("phase", phaseName(sim.phase()));
    j.open("lap")
        .integer("n", t.lap)
        .num("time", t.lapTimeS, 2)
        .num("last", t.lastLapS, 2)
        .num("best", t.bestLapS, 2)
        .num("dist", t.distanceM, 1)
        .close();
    j.open("car")
        .num("kmh", t.speedKmh, 1)
        .integer("gear", t.gear)
        .num("latG", t.latG, 2)
        .num("longG", t.longG, 2)
        .num("brake", t.brake * 100, 0)
        .num("pedal", t.throttle, 1);
#ifdef TRACK_OUTLINE
    float x, y;
    track::position(t.distanceM, x, y);
    j.num("x", x, 1).num("y", y, 1);
#endif
    j.close();
    j.open("ecu")
        .integer("rpm", s.rpm())
        .integer("map", s.mapKpa())
        .num("tps", s.tpsPercent(), 1)
        .integer("clt", s.coolantC())
        .integer("iat", s.intakeC())
        .num("afr", s.o2 / 10.0f, 1)
        .num("afrTarget", s.afrTarget / 10.0f, 1)
        .integer("adv", s.advance)
        .num("pw", s.pulseWidthUs() / 1000.0f, 2)
        .num("bat", s.battery10 / 10.0f, 1)
        .integer("oil", s.oilPressure)
        .integer("fuelP", s.fuelPressure)
        .integer("ego", s.egoCorrection)
        .integer("gammaE", EngineStatus::get16(s.correctionsLo, s.correctionsHi))
        .integer("ve", s.ve)
        .integer("idle", s.idleLoad)
        .integer("idleTarget", s.clIdleTarget * 10)
        .num("oilC", t.oilC, 0)
        .integer("engine", s.engine)
        .integer("status1", s.status1)
        .integer("status2", s.status2)
        .integer("status4", s.status4)
        .close();
    j.close();
    return j.overflow() ? 0 : j.length();
}

size_t packetJson(char* buf, size_t cap, const EngineSimulator& sim) {
    const uint8_t* raw = (const uint8_t*)&sim.getStatus();
    JsonWriter j(buf, cap);
    j.open().openArray("bytes");
    for (uint8_t i = 0; i < sizeof(EngineStatus); i++) j.integer(nullptr, raw[i]);
    j.closeArray().close();
    return j.overflow() ? 0 : j.length();
}

size_t statusJson(char* buf, size_t cap, EngineSimulator& sim, const ProtocolStats& p, const DeviceInfo& d) {
    SimSettings& cfg = sim.settings();
    JsonWriter j(buf, cap);
    j.open();
    j.str("firmware", FIRMWARE_VERSION).str("signature", SPEEDUINO_SIGNATURE).str("product", SPEEDUINO_PRODUCT);
    j.str("platform", d.platform).integer("uptime", (long)sim.uptimeS()).integer("freeHeap", (long)d.freeHeap);
    j.str("phase", phaseName(sim.phase()));
    j.str("track", track::name()).num("trackLength", track::length(), 0);
    j.open("wifi").str("mode", d.wifiMode).str("ip", d.ip).str("ssid", d.ssid).integer("rssi", d.rssi).close();
    j.open("serial")
        .str("transport", d.serialTransport)
        .boolean("client", d.serialClient)
        .integer("commands", (long)p.commands)
        .integer("framed", (long)p.framed)
        .integer("realtime", (long)p.realtimeReads)
        .integer("crcErrors", (long)p.crcErrors)
        .integer("unknown", (long)p.unknown)
        .integer("timeouts", (long)p.timeouts)
        .integer("lastAgoMs", p.commands ? (long)(d.nowMs - p.lastCommandMs) : -1);
    char last[2] = {p.lastCommand >= 32 && p.lastCommand < 127 ? p.lastCommand : '\0', '\0'};
    j.str("last", last).close();
    j.integer("streamClients", d.streamClients);
    j.open("settings")
        .num("pace", cfg.pace, 2)
        .integer("hotLaps", cfg.hotLaps)
        .num("ambientC", cfg.ambientC, 0)
        .num("startCoolantC", cfg.startCoolantC, 0)
        .boolean("autoStart", cfg.autoStart)
        .boolean("launchControl", cfg.launchControl)
        .boolean("sensorNoise", cfg.sensorNoise)
        .close();
    j.close();
    return j.overflow() ? 0 : j.length();
}

#ifdef TRACK_OUTLINE
size_t trackJson(char* buf, size_t cap) {
    JsonWriter j(buf, cap);
    j.open().str("name", track::name()).num("length", track::length(), 1);
    j.num("originLat", track::originLat(), 7).num("originLon", track::originLon(), 7);
    j.str("attribution", "© OpenStreetMap contributors, ODbL-1.0");
    j.openArray("points");
    for (uint16_t i = 0; i < track::outlinePoints(); i++) {
        int16_t x, y;
        track::outlinePoint(i, x, y);
        j.integer(nullptr, x).integer(nullptr, y);
    }
    j.closeArray().close();
    return j.overflow() ? 0 : j.length();
}
#endif
