/**
 * Host tests: packet encoding, protocol framing against comms.cpp behaviour,
 * the lap model's calibration targets, and the real CORE4 capture.
 *
 *   pio test -e native
 */
#include <unity.h>
#include <string.h>
#include <deque>
#include <string>
#include <vector>
#include "EngineSimulator.h"
#include "SpeeduinoProtocol.h"
#include "Track.h"
#include "Snapshot.h"
#include "core4_capture.h"

// ---------------------------------------------------------------- fakes

class FakeTime : public ITimeProvider {
public:
    uint32_t ms = 1000;
    uint32_t millis() override { return ms; }
    uint32_t micros() override { return ms * 1000; }
    void delay(uint32_t d) override { ms += d; }
    void delayMicroseconds(uint32_t us) override { waitedUs += us; }
    uint32_t waitedUs = 0;
};

class FakeRandom : public IRandomProvider {
public:
    uint32_t state = 86;
    void seed(uint32_t s) override { state = s ? s : 1; }
    int32_t random(int32_t lo, int32_t hi) override {
        state = state * 1664525u + 1013904223u;
        return hi <= lo ? lo : lo + (int32_t)(state % (uint32_t)(hi - lo));
    }
    int32_t random(int32_t hi) override { return random(0, hi); }
};

class MockSerial : public ISerialInterface {
public:
    std::deque<uint8_t> in;
    std::vector<uint8_t> out;
    void begin(uint32_t) override {}
    bool isReady() override { return true; }
    int available() override { return (int)in.size(); }
    int read() override {
        if (in.empty()) return -1;
        int b = in.front();
        in.pop_front();
        return b;
    }
    size_t readBytes(uint8_t* b, size_t n) override {
        size_t i = 0;
        while (i < n && !in.empty()) b[i++] = (uint8_t)read();
        return i;
    }
    size_t write(uint8_t b) override { out.push_back(b); return 1; }
    size_t write(const uint8_t* b, size_t n) override { out.insert(out.end(), b, b + n); return n; }
    void flush() override {}
    void clear() override { in.clear(); }
    void send(const std::string& s) { for (char c : s) in.push_back((uint8_t)c); }
    void sendFrame(const std::vector<uint8_t>& payload) {
        in.push_back((uint8_t)(payload.size() >> 8));
        in.push_back((uint8_t)payload.size());
        for (uint8_t b : payload) in.push_back(b);
        uint32_t c = SpeeduinoProtocol::crc32(payload.data(), payload.size());
        for (int s = 24; s >= 0; s -= 8) in.push_back((uint8_t)(c >> s));
    }
    /// Decode one framed response from `out`, checking its CRC.
    std::vector<uint8_t> frame() {
        TEST_ASSERT_TRUE_MESSAGE(out.size() >= 6, "short frame");
        size_t len = (size_t)out[0] << 8 | out[1];
        TEST_ASSERT_EQUAL_UINT(len + 6, out.size());
        std::vector<uint8_t> payload(out.begin() + 2, out.begin() + 2 + len);
        uint32_t c = (uint32_t)out[2 + len] << 24 | (uint32_t)out[3 + len] << 16 | (uint32_t)out[4 + len] << 8 | out[5 + len];
        TEST_ASSERT_EQUAL_HEX32(SpeeduinoProtocol::crc32(payload.data(), payload.size()), c);
        out.clear();
        return payload;
    }
};

struct Rig {
    FakeTime time;
    FakeRandom rng;
    MockSerial line;
    EngineSimulator sim{&time, &rng};
    SpeeduinoProtocol proto{&line, &sim, &time};
    Rig() {
        sim.begin();
        proto.begin();
        proto.wirePacing = false;  // the fake clock does not advance while waiting
    }
    void run(float seconds) {
        for (uint32_t i = 0; i < (uint32_t)(seconds * 1000 / SIM_STEP_MS); i++) {
            time.ms += SIM_STEP_MS;
            sim.update();
        }
    }
};

static float pwMs(const EngineStatus& s) { return s.pulseWidthUs() / 1000.0f; }
static int16_t s16(uint8_t lo, uint8_t hi) { return (int16_t)(lo | hi << 8); }

// ---------------------------------------------------------------- packet

void test_setters_encode_firmware_units() {
    EngineStatus s{};
    s.setTpsPercent(61.5f);
    TEST_ASSERT_EQUAL_UINT8(123, s.tps);  // 0.5 % steps, as the CORE4 sends
    s.setPulseWidthUs(3250);
    TEST_ASSERT_EQUAL_UINT16(3250, s.pulseWidthUs());
    TEST_ASSERT_EQUAL_UINT8(3250 & 0xFF, s.pw4Lo);
    s.setTpsDot(-250);
    TEST_ASSERT_EQUAL_INT16(-250, s16(s.tpsDotLo, s.tpsDotHi));
    s.setCoolantC(-5);
    TEST_ASSERT_EQUAL_UINT8(35, s.clt);
    s.setAfr(14.7f);
    TEST_ASSERT_EQUAL_UINT8(147, s.o2);
}

void test_crc32_reference_vector() {
    const char* v = "123456789";
    TEST_ASSERT_EQUAL_HEX32(0xCBF43926, SpeeduinoProtocol::crc32((const uint8_t*)v, 9));
}

// ---------------------------------------------------------------- legacy

void test_legacy_identity_matches_the_core4() {
    Rig r;
    r.line.send("Q");
    r.proto.poll();
    TEST_ASSERT_EQUAL_STRING("speeduino 202501", std::string(r.line.out.begin(), r.line.out.end()).c_str());
    r.line.out.clear();
    r.line.send("S");
    r.proto.poll();
    TEST_ASSERT_EQUAL_STRING("Speeduino 2025.01.6", std::string(r.line.out.begin(), r.line.out.end()).c_str());
    r.line.out.clear();
    r.line.send("F");
    r.proto.poll();
    TEST_ASSERT_EQUAL_STRING("002", std::string(r.line.out.begin(), r.line.out.end()).c_str());
}

void test_legacy_A_returns_the_130_byte_packet() {
    Rig r;
    r.run(5);
    r.line.send("A");
    r.proto.poll();
    TEST_ASSERT_EQUAL_UINT(130, r.line.out.size());
    TEST_ASSERT_EQUAL_MEMORY(&r.sim.getStatus(), r.line.out.data(), 130);
}

void test_legacy_r_reads_a_window() {
    Rig r;
    r.run(5);
    r.line.in.push_back('r');
    for (uint8_t b : {0x00, 0x30, 14, 0, 2, 0}) r.line.in.push_back(b);  // rpm, 2 bytes
    r.proto.poll();
    TEST_ASSERT_EQUAL_UINT(2, r.line.out.size());
    TEST_ASSERT_EQUAL_UINT8(r.sim.getStatus().rpmLo, r.line.out[0]);
}

// ---------------------------------------------------------------- framed

void test_framed_Q_and_C() {
    Rig r;
    r.line.sendFrame({'Q'});
    r.proto.poll();
    auto q = r.line.frame();
    TEST_ASSERT_EQUAL_UINT8(rc::OK, q[0]);
    TEST_ASSERT_EQUAL_STRING("speeduino 202501", std::string(q.begin() + 1, q.end()).c_str());
    r.line.sendFrame({'C'});
    r.proto.poll();
    auto c = r.line.frame();
    TEST_ASSERT_EQUAL_UINT(2, c.size());
    TEST_ASSERT_EQUAL_UINT8(255, c[1]);
}

void test_framed_bad_crc_and_unknown() {
    Rig r;
    r.line.in.insert(r.line.in.end(), {0x00, 0x01, 'Q', 1, 2, 3, 4});
    r.proto.poll();
    TEST_ASSERT_EQUAL_UINT8(rc::CRC_ERR, r.line.frame()[0]);
    r.line.sendFrame({'#'});
    r.proto.poll();
    TEST_ASSERT_EQUAL_UINT8(rc::UNKNOWN, r.line.frame()[0]);
    TEST_ASSERT_EQUAL_UINT32(1, r.proto.stats().crcErrors);
}

void test_framed_output_channels() {
    Rig r;
    r.run(5);
    r.line.sendFrame({'r', 0x00, 0x30, 0, 0, 130, 0});
    r.proto.poll();
    auto p = r.line.frame();
    TEST_ASSERT_EQUAL_UINT(131, p.size());
    TEST_ASSERT_EQUAL_MEMORY(&r.sim.getStatus(), p.data() + 1, 130);
}

void test_framed_S_resets_secl() {
    Rig r;
    r.run(10);
    TEST_ASSERT_TRUE(r.sim.getStatus().secl >= 9);
    r.line.sendFrame({'S'});
    r.proto.poll();
    r.line.frame();
    r.run(0.05f);
    TEST_ASSERT_TRUE(r.sim.getStatus().secl <= 1);
    TEST_ASSERT_EQUAL_UINT8(0, r.sim.getStatus().status5 & 0x20);
}

void test_page_write_read_and_crc_agree() {
    Rig r;
    r.line.sendFrame({'M', 0, 2, 4, 0, 3, 0, 0xAA, 0xBB, 0xCC});  // page 2, offset 4, 3 bytes
    r.proto.poll();
    TEST_ASSERT_EQUAL_UINT8(rc::OK, r.line.frame()[0]);
    r.line.sendFrame({'p', 0, 2, 0, 0, 32, 1});  // whole page 2 (288 bytes)
    r.proto.poll();
    auto page = r.line.frame();
    TEST_ASSERT_EQUAL_UINT(289, page.size());
    TEST_ASSERT_EQUAL_UINT8(0xBB, page[1 + 5]);
    r.line.sendFrame({'d', 0, 2});
    r.proto.poll();
    auto d = r.line.frame();
    uint32_t c = (uint32_t)d[1] << 24 | (uint32_t)d[2] << 16 | (uint32_t)d[3] << 8 | d[4];
    TEST_ASSERT_EQUAL_HEX32(SpeeduinoProtocol::crc32(page.data() + 1, 288), c);  // big-endian, as comms.cpp
}

void test_replies_take_as_long_as_a_115200_baud_uart() {
    Rig r;
    r.proto.wirePacing = true;
    r.line.send("A");
    r.proto.poll();
    TEST_ASSERT_EQUAL_UINT(130, r.line.out.size());
    // 130 bytes × 10 bits at 115200 baud ≈ 11.3 ms, as the CORE4 (13.8 ms measured with USB latency).
    TEST_ASSERT_INT_WITHIN(200, 130 * 10000000 / 115200 + 300, r.time.waitedUs);
}

void test_capabilities_report_avr_blocking_factors() {
    Rig r;
    r.line.sendFrame({'f'});
    r.proto.poll();
    auto f = r.line.frame();
    TEST_ASSERT_EQUAL_UINT(6, f.size());
    TEST_ASSERT_EQUAL_UINT8(2, f[1]);
    TEST_ASSERT_EQUAL_UINT8(121, f[3]);
    TEST_ASSERT_EQUAL_UINT8(64, f[5]);
}

// ---------------------------------------------------------------- model

void test_engine_off_matches_the_core4_bench_capture() {
    Rig r;
    r.sim.settings().autoStart = false;
    r.sim.begin();
    r.run(1);
    const EngineStatus& s = r.sim.getStatus();
    const uint8_t* real = test_fixtures_core4_202501_engine_off_bin;
    TEST_ASSERT_EQUAL_UINT16(0, s.rpm());
    TEST_ASSERT_EQUAL_UINT8(0, s.engine);
    TEST_ASSERT_EQUAL_UINT8(real[110], s.status4);  // allowLegacyComms
    TEST_ASSERT_EQUAL_UINT8(real[127], s.status5);  // TS allowed on secondary
    TEST_ASSERT_EQUAL_UINT8(real[116], s.fuelTemp); // no flex sensor
    TEST_ASSERT_EQUAL_UINT16(real[28] | real[29] << 8, s.freeRamLo | s.freeRamHi << 8);
    TEST_ASSERT_INT_WITHIN(2, real[41], s.baro);
    TEST_ASSERT_INT_WITHIN(150, real[26] | real[27] << 8, s.loopsLo | s.loopsHi << 8);
    TEST_ASSERT_EQUAL_UINT8(0, s.wueCorrection);    // corrections are not computed while stopped
    TEST_ASSERT_EQUAL_UINT8(0, s.afrTarget);
}

void test_cold_start_settles_to_idle() {
    Rig r;
    r.run(2.5f);
    TEST_ASSERT_EQUAL(Phase::Cranking, r.sim.phase());
    TEST_ASSERT_TRUE(r.sim.getStatus().engine & bits::CRANKING);
    r.run(12);
    const EngineStatus& s = r.sim.getStatus();
    TEST_ASSERT_EQUAL(Phase::WarmUp, r.sim.phase());
    TEST_ASSERT_INT_WITHIN(150, 950, s.rpm());
    TEST_ASSERT_INT_WITHIN(8, 35, s.mapKpa());
    TEST_ASSERT_TRUE(s.engine & bits::RUNNING);
    TEST_ASSERT_TRUE(s.status2 & bits::SYNC);
    TEST_ASSERT_TRUE(s.status2 & bits::IDLE_CONTROL);
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 2.4f, pwMs(s));
    TEST_ASSERT_EQUAL_UINT8(0, s.tps);
    TEST_ASSERT_EQUAL_UINT8(2 << 5, s.status3 & 0xE0);  // 2 squirts
}

void test_a_session_laps_biķernieki_like_a_real_car() {
    Rig r;
    float maxRpm = 0, maxPw = 0, maxClt = 0, maxLatG = 0;
    uint8_t maxGear = 0;
    bool dfco = false, launch = false, wot = false;
    for (int i = 0; i < 9 * 60 * 100 && r.sim.telemetry().bestLapS == 0; i++) {
        r.run(0.01f);
        const EngineStatus& s = r.sim.getStatus();
        const Telemetry& t = r.sim.telemetry();
        if (s.rpm() > maxRpm) maxRpm = s.rpm();
        if (pwMs(s) > maxPw && !(s.engine & bits::TPS_ACCEL)) maxPw = pwMs(s);
        if (s.coolantC() > maxClt) maxClt = s.coolantC();
        if (t.gear > maxGear) maxGear = t.gear;
        if (fabsf(t.latG) > maxLatG) maxLatG = fabsf(t.latG);
        dfco |= (s.status1 & bits::DFCO) && s.pulseWidthUs() == 0;
        launch |= (s.status2 & bits::LAUNCH_SOFT) != 0;
        wot |= s.tps == 200 && s.mapKpa() >= 97;
    }
    const Telemetry& t = r.sim.telemetry();
    TEST_ASSERT_TRUE_MESSAGE(t.bestLapS > 0, "no hot lap completed");
    TEST_ASSERT_FLOAT_WITHIN(12.0f, 122.0f, t.bestLapS);
    TEST_ASSERT_TRUE(maxRpm < 7350);
    TEST_ASSERT_TRUE(maxRpm > 6500);
    TEST_ASSERT_TRUE(maxGear >= 4);
    TEST_ASSERT_TRUE_MESSAGE(dfco, "no deceleration fuel cut");
    TEST_ASSERT_TRUE_MESSAGE(launch, "no launch control");
    TEST_ASSERT_TRUE_MESSAGE(wot, "never at wide-open throttle");
    TEST_ASSERT_FLOAT_WITHIN(1.8f, 7.0f, maxPw);
    TEST_ASSERT_TRUE(maxClt < 104);
    TEST_ASSERT_TRUE(maxLatG > 0.6f && maxLatG < 1.4f);
}

// ---------------------------------------------------------------- track

void test_track_wraps_and_closes() {
    TEST_ASSERT_FLOAT_WITHIN(10, 3647, track::length());
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 5.0f, track::wrap(track::length() + 5.0f));
    for (float s = 0; s < track::length(); s += 7) {
        float v = track::envelope(s) * 3.6f;
        TEST_ASSERT_TRUE(v >= 50 && v <= 231);
    }
    float x0, y0, x1, y1;
    track::position(0, x0, y0);
    track::position(track::length() - 0.5f, x1, y1);
    TEST_ASSERT_TRUE(fabsf(x0 - x1) < 6 && fabsf(y0 - y1) < 6);
}

// ---------------------------------------------------------------- web JSON

static bool balanced(const char* j) {
    int depth = 0;
    bool quoted = false;
    for (const char* c = j; *c; c++) {
        if (*c == '\\' && quoted) { c++; continue; }
        if (*c == '"') quoted = !quoted;
        else if (!quoted && (*c == '{' || *c == '[')) depth++;
        else if (!quoted && (*c == '}' || *c == ']')) depth--;
        if (depth < 0) return false;
    }
    return depth == 0 && !quoted;
}

void test_web_documents_fit_their_buffers() {
    Rig r;
    r.run(30);
    static char buf[4096];
    TEST_ASSERT_TRUE(liveJson(buf, 1024, r.sim) > 0);
    TEST_ASSERT_TRUE(balanced(buf));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"phase\":\"warm-up\""));
    TEST_ASSERT_TRUE(packetJson(buf, 1024, r.sim) > 0);
    TEST_ASSERT_TRUE(balanced(buf));
    DeviceInfo d;
    d.ssid = "quote\" and \\ back";
    TEST_ASSERT_TRUE(statusJson(buf, 1536, r.sim, r.proto.stats(), d) > 0);
    TEST_ASSERT_TRUE(balanced(buf));
    TEST_ASSERT_NOT_NULL(strstr(buf, "quote\\\" and \\\\ back"));
    TEST_ASSERT_TRUE(trackJson(buf, sizeof(buf)) > 0);
    TEST_ASSERT_TRUE(balanced(buf));
    TEST_ASSERT_EQUAL(0, liveJson(buf, 40, r.sim));  // overflow reports 0, never truncated JSON
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_setters_encode_firmware_units);
    RUN_TEST(test_crc32_reference_vector);
    RUN_TEST(test_legacy_identity_matches_the_core4);
    RUN_TEST(test_legacy_A_returns_the_130_byte_packet);
    RUN_TEST(test_legacy_r_reads_a_window);
    RUN_TEST(test_framed_Q_and_C);
    RUN_TEST(test_framed_bad_crc_and_unknown);
    RUN_TEST(test_framed_output_channels);
    RUN_TEST(test_framed_S_resets_secl);
    RUN_TEST(test_page_write_read_and_crc_agree);
    RUN_TEST(test_replies_take_as_long_as_a_115200_baud_uart);
    RUN_TEST(test_capabilities_report_avr_blocking_factors);
    RUN_TEST(test_engine_off_matches_the_core4_bench_capture);
    RUN_TEST(test_cold_start_settles_to_idle);
    RUN_TEST(test_a_session_laps_biķernieki_like_a_real_car);
    RUN_TEST(test_track_wraps_and_closes);
    RUN_TEST(test_web_documents_fit_their_buffers);
    return UNITY_END();
}
