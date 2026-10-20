/**
 * @file EngineStatus.h
 * @brief Speeduino 202501 realtime packet: the 130 bytes `A` and `r` return.
 *
 * Byte order follows getTSLogEntry() in speeduino/logger.cpp (release
 * 202501.6). Units follow the firmware, not older wiki pages:
 *   - TPS is 0.5 % steps (0–200) since 202201 (sensors.cpp maps to 0..200).
 *   - PW1–PW4, dwell and actualDwell are microseconds.
 *   - tpsDOT (%/s), mapDOT (kPa/s), rpmDOT and fuel/ignition load are signed.
 *   - O2 and afrTarget are AFR × 10; oil and fuel pressure are PSI.
 *   - VVT angles/targets/duty and fan duty are 0.5 steps.
 * The setters below take physical units and do the encoding, so every
 * caller writes the same thing a real controller sends.
 */
#pragma once

#include <stdint.h>
#include <stddef.h>

#pragma pack(push, 1)

struct EngineStatus {
    /* 0  */ uint8_t secl;
    /* 1  */ uint8_t status1;           ///< inj1-4 (0-3), DFCO (4), boostCutFuel (5), toothLog (6-7)
    /* 2  */ uint8_t engine;            ///< running, crank, ase, warmup, tpsaccaen, tpsaccden, mapaccaen, mapaccden
    /* 3  */ uint8_t syncLossCounter;
    /* 4  */ uint8_t mapLo, mapHi;      ///< kPa
    /* 6  */ uint8_t iat;               ///< °C + 40
    /* 7  */ uint8_t clt;               ///< °C + 40
    /* 8  */ uint8_t batCorrection;     ///< %
    /* 9  */ uint8_t battery10;         ///< V × 10
    /* 10 */ uint8_t o2;                ///< AFR × 10
    /* 11 */ uint8_t egoCorrection;     ///< %
    /* 12 */ uint8_t iatCorrection;     ///< %
    /* 13 */ uint8_t wueCorrection;     ///< %
    /* 14 */ uint8_t rpmLo, rpmHi;
    /* 16 */ uint8_t aeAmount;          ///< AEamount >> 1 (100 % = no enrichment)
    /* 17 */ uint8_t correctionsLo, correctionsHi;  ///< gammaE, %
    /* 19 */ uint8_t ve1;
    /* 20 */ uint8_t ve2;
    /* 21 */ uint8_t afrTarget;         ///< AFR × 10
    /* 22 */ uint8_t tpsDotLo, tpsDotHi;  ///< %/s, signed
    /* 24 */ int8_t advance;            ///< ° BTDC
    /* 25 */ uint8_t tps;               ///< 0.5 % steps
    /* 26 */ uint8_t loopsLo, loopsHi;
    /* 28 */ uint8_t freeRamLo, freeRamHi;
    /* 30 */ uint8_t boostTarget;       ///< kPa >> 1
    /* 31 */ uint8_t boostDuty;         ///< %
    /* 32 */ uint8_t status2;           ///< launchHard, launchSoft, hardLimit, softLimit, boostCutSpark, error, idleControlOn, sync
    /* 33 */ uint8_t rpmDotLo, rpmDotHi;  ///< rpm/s, signed
    /* 35 */ uint8_t ethanolPct;
    /* 36 */ uint8_t flexCorrection;
    /* 37 */ int8_t flexIgnCorrection;
    /* 38 */ uint8_t idleLoad;          ///< idle valve duty, %
    /* 39 */ uint8_t testOutputs;
    /* 40 */ uint8_t o2_2;              ///< AFR × 10
    /* 41 */ uint8_t baro;              ///< kPa
    /* 42 */ uint8_t canin[32];         ///< 16 × uint16 LE
    /* 74 */ uint8_t tpsADC;
    /* 75 */ uint8_t errors;
    /* 76 */ uint8_t pw1Lo, pw1Hi;      ///< µs
    /* 78 */ uint8_t pw2Lo, pw2Hi;
    /* 80 */ uint8_t pw3Lo, pw3Hi;
    /* 82 */ uint8_t pw4Lo, pw4Hi;
    /* 84 */ uint8_t status3;           ///< resetLock, nitrous, fuel2, vssRefresh, halfSync, nSquirts (5-7)
    /* 85 */ uint8_t engineProtectStatus;
    /* 86 */ uint8_t fuelLoadLo, fuelLoadHi;  ///< signed, kPa (speed density)
    /* 88 */ uint8_t ignLoadLo, ignLoadHi;
    /* 90 */ uint8_t dwellLo, dwellHi;  ///< µs
    /* 92 */ uint8_t clIdleTarget;      ///< rpm / 10
    /* 93 */ uint8_t mapDotLo, mapDotHi;  ///< kPa/s, signed
    /* 95 */ uint8_t vvt1AngleLo, vvt1AngleHi;
    /* 97 */ uint8_t vvt1TargetAngle;
    /* 98 */ uint8_t vvt1Duty;
    /* 99 */ uint8_t flexBoostCorrLo, flexBoostCorrHi;
    /* 101 */ uint8_t baroCorrection;   ///< %
    /* 102 */ uint8_t ve;               ///< current VE, %
    /* 103 */ uint8_t aseValue;         ///< %
    /* 104 */ uint8_t vssLo, vssHi;     ///< km/h
    /* 106 */ uint8_t gear;
    /* 107 */ uint8_t fuelPressure;     ///< PSI
    /* 108 */ uint8_t oilPressure;      ///< PSI
    /* 109 */ uint8_t wmiPW;
    /* 110 */ uint8_t status4;          ///< wmiEmpty, vvt1Err, vvt2Err, fan, burnPending, staging, commsCompat, allowLegacyComms
    /* 111 */ uint8_t vvt2AngleLo, vvt2AngleHi;
    /* 113 */ uint8_t vvt2TargetAngle;
    /* 114 */ uint8_t vvt2Duty;
    /* 115 */ uint8_t outputsStatus;
    /* 116 */ uint8_t fuelTemp;         ///< °C + 40
    /* 117 */ uint8_t fuelTempCorrection;
    /* 118 */ int8_t advance1;
    /* 119 */ int8_t advance2;
    /* 120 */ uint8_t tsSdStatus;
    /* 121 */ uint8_t emapLo, emapHi;
    /* 123 */ uint8_t fanDuty;          ///< 0.5 % steps
    /* 124 */ uint8_t airConStatus;
    /* 125 */ uint8_t actualDwellLo, actualDwellHi;  ///< µs
    /* 127 */ uint8_t status5;
    /* 128 */ uint8_t knockCount;
    /* 129 */ uint8_t knockRetard;

    static void put16(uint8_t& lo, uint8_t& hi, uint16_t v) {
        lo = (uint8_t)(v & 0xFF);
        hi = (uint8_t)(v >> 8);
    }
    static uint16_t get16(uint8_t lo, uint8_t hi) { return (uint16_t)((uint16_t)hi << 8 | lo); }
    static uint8_t temp(float c) { return clampByte(c + 40.0f); }
    static uint8_t clampByte(float v) { return v <= 0 ? 0 : v >= 255 ? 255 : (uint8_t)(v + 0.5f); }
    static uint16_t clampU16(float v) { return v <= 0 ? 0 : v >= 65535 ? 65535 : (uint16_t)(v + 0.5f); }
    static int16_t clampS16(float v) {
        return v <= -32768 ? (int16_t)-32768 : v >= 32767 ? (int16_t)32767 : (int16_t)(v < 0 ? v - 0.5f : v + 0.5f);
    }

    void setRpm(float rpm) { put16(rpmLo, rpmHi, clampU16(rpm)); }
    uint16_t rpm() const { return get16(rpmLo, rpmHi); }
    void setMapKpa(float kpa) { put16(mapLo, mapHi, clampU16(kpa)); }
    uint16_t mapKpa() const { return get16(mapLo, mapHi); }
    void setTpsPercent(float pct) { tps = clampByte(pct * 2.0f > 200 ? 200 : pct * 2.0f); }
    float tpsPercent() const { return tps / 2.0f; }
    void setPulseWidthUs(float us) {
        uint16_t v = clampU16(us);
        put16(pw1Lo, pw1Hi, v);
        put16(pw2Lo, pw2Hi, v);
        put16(pw3Lo, pw3Hi, v);
        put16(pw4Lo, pw4Hi, v);
    }
    uint16_t pulseWidthUs() const { return get16(pw1Lo, pw1Hi); }
    void setDwellUs(float us, float actualUs) {
        put16(dwellLo, dwellHi, clampU16(us));
        put16(actualDwellLo, actualDwellHi, clampU16(actualUs));
    }
    void setCoolantC(float c) { clt = temp(c); }
    int16_t coolantC() const { return (int16_t)clt - 40; }
    void setIntakeC(float c) { iat = temp(c); }
    int16_t intakeC() const { return (int16_t)iat - 40; }
    void setAfr(float afr) { o2 = clampByte(afr * 10.0f); }
    void setAfrTarget(float afr) { afrTarget = clampByte(afr * 10.0f); }
    void setBattery(float volts) { battery10 = clampByte(volts * 10.0f); }
    void setTpsDot(float pctPerS) { put16(tpsDotLo, tpsDotHi, (uint16_t)clampS16(pctPerS)); }
    void setMapDot(float kpaPerS) { put16(mapDotLo, mapDotHi, (uint16_t)clampS16(kpaPerS)); }
    void setRpmDot(float rpmPerS) { put16(rpmDotLo, rpmDotHi, (uint16_t)clampS16(rpmPerS)); }
    void setLoads(float kpa) {
        put16(fuelLoadLo, fuelLoadHi, (uint16_t)clampS16(kpa));
        put16(ignLoadLo, ignLoadHi, (uint16_t)clampS16(kpa));
    }
    void setCorrections(float gammaE) { put16(correctionsLo, correctionsHi, clampU16(gammaE)); }
    void setLoops(uint16_t n) { put16(loopsLo, loopsHi, n); }
    void setFreeRam(uint16_t n) { put16(freeRamLo, freeRamHi, n); }
    void setVss(float kmh) { put16(vssLo, vssHi, clampU16(kmh)); }
    uint16_t vss() const { return get16(vssLo, vssHi); }
};

#pragma pack(pop)

static_assert(sizeof(EngineStatus) == 130, "EngineStatus must be exactly 130 bytes");
static_assert(offsetof(EngineStatus, tps) == 25, "TPS at byte 25");
static_assert(offsetof(EngineStatus, status2) == 32, "status2 at byte 32");
static_assert(offsetof(EngineStatus, pw1Lo) == 76, "PW1 at byte 76");
static_assert(offsetof(EngineStatus, dwellLo) == 90, "dwell at byte 90");
static_assert(offsetof(EngineStatus, vssLo) == 104, "VSS at byte 104");
static_assert(offsetof(EngineStatus, status4) == 110, "status4 at byte 110");
static_assert(offsetof(EngineStatus, actualDwellLo) == 125, "actualDwell at byte 125");

// Bit positions (speeduino/globals.h, 202501.6).
namespace bits {
// engine
constexpr uint8_t RUNNING = 1 << 0, CRANKING = 1 << 1, ASE = 1 << 2, WARMUP = 1 << 3,
                  TPS_ACCEL = 1 << 4, TPS_DECEL = 1 << 5;
// status1
constexpr uint8_t INJ1 = 1 << 0, DFCO = 1 << 4;
// status2
constexpr uint8_t LAUNCH_HARD = 1 << 0, LAUNCH_SOFT = 1 << 1, HARD_LIMIT = 1 << 2,
                  SOFT_LIMIT = 1 << 3, IDLE_CONTROL = 1 << 6, SYNC = 1 << 7;
// status4
constexpr uint8_t FAN = 1 << 3, ALLOW_LEGACY_COMMS = 1 << 7;
}  // namespace bits
