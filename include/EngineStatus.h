/**
 * @file EngineStatus.h
 * @brief Speeduino real-time output-channel data structure
 *
 * Byte layout matches getTSLogEntry() from Speeduino 202501 exactly.
 * TunerStudio requests these bytes via the framed 'r' command (sub-cmd 0x30)
 * and interprets them purely by byte position, not by field name.
 *
 * Reference: speeduino/logger.cpp  getTSLogEntry()  tag 202501
 *
 * Byte  Field
 * ----  -----
 *  0    secl
 *  1    status1
 *  2    engine
 *  3    syncLossCounter
 *  4-5  MAP  lo/hi  (kPa)
 *  6    IAT  + 40   (°C offset)
 *  7    CLT  + 40   (°C offset)
 *  8    batCorrection (%)
 *  9    battery10   (V × 10)
 * 10    O2
 * 11    egoCorrection (%)
 * 12    iatCorrection (%)
 * 13    wueCorrection (%)
 * 14-15 RPM  lo/hi
 * 16    AEamount >> 1
 * 17-18 corrections lo/hi  (gammaE, 16-bit %)
 * 19    VE1 (%)
 * 20    VE2 (%)
 * 21    afrTarget
 * 22-23 tpsDOT lo/hi
 * 24    advance (°BTDC)
 * 25    TPS (0-100 %)
 * 26-27 loopsPerSecond lo/hi
 * 28-29 freeRAM lo/hi
 * 30    boostTarget >> 1
 * 31    boostDuty / 100
 * 32    status2  (spark bitfield)
 * 33-34 rpmDOT lo/hi
 * 35    ethanolPct (%)
 * 36    flexCorrection (%)
 * 37    flexIgnCorrection (°)
 * 38    idleLoad
 * 39    testOutputs
 * 40    O2_2
 * 41    baro (kPa)
 * 42-73 canin[0..15] lo/hi  (32 bytes)
 * 74    tpsADC
 * 75    errors
 * 76-77 PW1 lo/hi  (µs / 100)
 * 78-79 PW2 lo/hi
 * 80-81 PW3 lo/hi
 * 82-83 PW4 lo/hi
 * 84    status3
 * 85    engineProtectStatus
 * 86-87 fuelLoad lo/hi
 * 88-89 ignLoad  lo/hi
 * 90-91 dwell    lo/hi
 * 92    CLIdleTarget
 * 93-94 mapDOT lo/hi
 * 95-96 vvt1Angle lo/hi
 * 97    vvt1TargetAngle
 * 98    vvt1Duty
 * 99-100 flexBoostCorrection lo/hi
 * 101   baroCorrection
 * 102   VE  (current, %)
 * 103   ASEValue (%)
 * 104-105 vss lo/hi
 * 106   gear
 * 107   fuelPressure
 * 108   oilPressure
 * 109   wmiPW
 * 110   status4
 * 111-112 vvt2Angle lo/hi
 * 113   vvt2TargetAngle
 * 114   vvt2Duty
 * 115   outputsStatus
 * 116   fuelTemp + 40
 * 117   fuelTempCorrection
 * 118   advance1
 * 119   advance2
 * 120   tsSdStatus
 * 121-122 EMAP lo/hi
 * 123   fanDuty
 * 124   airConStatus
 * 125-126 actualDwell lo/hi
 * 127   status5
 * 128   knockCount
 * 129   knockRetard
 */

#ifndef ENGINE_STATUS_H
#define ENGINE_STATUS_H

#include <stdint.h>

#pragma pack(push, 1)

struct EngineStatus {
    /* 0  */ uint8_t  secl;
    /* 1  */ uint8_t  status1;
    /* 2  */ uint8_t  engine;
    /* 3  */ uint8_t  syncLossCounter;
    /* 4  */ uint8_t  mapLo;
    /* 5  */ uint8_t  mapHi;
    /* 6  */ uint8_t  iat;               ///< IAT (°C + 40)
    /* 7  */ uint8_t  clt;               ///< CLT (°C + 40)
    /* 8  */ uint8_t  batCorrection;     ///< Battery voltage correction (%)
    /* 9  */ uint8_t  battery10;         ///< Battery voltage × 10 (e.g. 140 = 14.0 V)
    /* 10 */ uint8_t  o2;
    /* 11 */ uint8_t  egoCorrection;     ///< EGO/O2 correction (%)
    /* 12 */ uint8_t  iatCorrection;     ///< IAT correction (%)
    /* 13 */ uint8_t  wueCorrection;     ///< Warm-up enrichment (%)
    /* 14 */ uint8_t  rpmLo;
    /* 15 */ uint8_t  rpmHi;
    /* 16 */ uint8_t  aeAmount;          ///< Accel enrichment / 2  (AEamount >> 1)
    /* 17 */ uint8_t  correctionsLo;     ///< gammaE total correction lo byte (%)
    /* 18 */ uint8_t  correctionsHi;     ///< gammaE total correction hi byte
    /* 19 */ uint8_t  ve1;               ///< VE table 1 (%)
    /* 20 */ uint8_t  ve2;               ///< VE table 2 (%)
    /* 21 */ uint8_t  afrTarget;         ///< Target AFR (AFR × 10)
    /* 22 */ uint8_t  tpsDotLo;          ///< TPS rate of change lo (%/s)
    /* 23 */ uint8_t  tpsDotHi;
    /* 24 */ uint8_t  advance;           ///< Ignition advance (°BTDC)
    /* 25 */ uint8_t  tps;               ///< TPS (0-100 %)
    /* 26 */ uint8_t  loopsLo;
    /* 27 */ uint8_t  loopsHi;
    /* 28 */ uint8_t  freeRamLo;
    /* 29 */ uint8_t  freeRamHi;
    /* 30 */ uint8_t  boostTarget;       ///< Boost target kPa / 2
    /* 31 */ uint8_t  boostDuty;         ///< Boost duty cycle / 100
    /* 32 */ uint8_t  status2;           ///< Spark / status2 bitfield
    /* 33 */ uint8_t  rpmDotLo;
    /* 34 */ uint8_t  rpmDotHi;
    /* 35 */ uint8_t  ethanolPct;
    /* 36 */ uint8_t  flexCorrection;
    /* 37 */ uint8_t  flexIgnCorrection;
    /* 38 */ uint8_t  idleLoad;
    /* 39 */ uint8_t  testOutputs;
    /* 40 */ uint8_t  o2_2;
    /* 41 */ uint8_t  baro;              ///< Baro pressure (kPa)
    /* 42 */ uint8_t  canin[32];         ///< CAN inputs: 16 × uint16 lo/hi pairs
    /* 74 */ uint8_t  tpsADC;
    /* 75 */ uint8_t  errors;
    /* 76 */ uint8_t  pw1Lo;             ///< Injector PW1 lo (µs/100)
    /* 77 */ uint8_t  pw1Hi;
    /* 78 */ uint8_t  pw2Lo;
    /* 79 */ uint8_t  pw2Hi;
    /* 80 */ uint8_t  pw3Lo;
    /* 81 */ uint8_t  pw3Hi;
    /* 82 */ uint8_t  pw4Lo;
    /* 83 */ uint8_t  pw4Hi;
    /* 84 */ uint8_t  status3;
    /* 85 */ uint8_t  engineProtectStatus;
    /* 86 */ uint8_t  fuelLoadLo;
    /* 87 */ uint8_t  fuelLoadHi;
    /* 88 */ uint8_t  ignLoadLo;
    /* 89 */ uint8_t  ignLoadHi;
    /* 90 */ uint8_t  dwellLo;           ///< Dwell lo (0.1 ms units)
    /* 91 */ uint8_t  dwellHi;
    /* 92 */ uint8_t  clIdleTarget;
    /* 93 */ uint8_t  mapDotLo;
    /* 94 */ uint8_t  mapDotHi;
    /* 95 */ uint8_t  vvt1AngleLo;
    /* 96 */ uint8_t  vvt1AngleHi;
    /* 97 */ uint8_t  vvt1TargetAngle;
    /* 98 */ uint8_t  vvt1Duty;
    /* 99 */ uint8_t  flexBoostCorrLo;
    /* 100*/ uint8_t  flexBoostCorrHi;
    /* 101*/ uint8_t  baroCorrection;
    /* 102*/ uint8_t  ve;                ///< Current VE (may differ from ve1/ve2)
    /* 103*/ uint8_t  aseValue;
    /* 104*/ uint8_t  vssLo;
    /* 105*/ uint8_t  vssHi;
    /* 106*/ uint8_t  gear;
    /* 107*/ uint8_t  fuelPressure;
    /* 108*/ uint8_t  oilPressure;
    /* 109*/ uint8_t  wmiPW;
    /* 110*/ uint8_t  status4;
    /* 111*/ uint8_t  vvt2AngleLo;
    /* 112*/ uint8_t  vvt2AngleHi;
    /* 113*/ uint8_t  vvt2TargetAngle;
    /* 114*/ uint8_t  vvt2Duty;
    /* 115*/ uint8_t  outputsStatus;
    /* 116*/ uint8_t  fuelTemp;          ///< Fuel temp (°C + 40)
    /* 117*/ uint8_t  fuelTempCorrection;
    /* 118*/ uint8_t  advance1;
    /* 119*/ uint8_t  advance2;
    /* 120*/ uint8_t  tsSdStatus;
    /* 121*/ uint8_t  emapLo;
    /* 122*/ uint8_t  emapHi;
    /* 123*/ uint8_t  fanDuty;
    /* 124*/ uint8_t  airConStatus;
    /* 125*/ uint8_t  actualDwellLo;
    /* 126*/ uint8_t  actualDwellHi;
    /* 127*/ uint8_t  status5;
    /* 128*/ uint8_t  knockCount;
    /* 129*/ uint8_t  knockRetard;

    // ---- Inline helpers ----

    void setRPM(uint16_t rpm) {
        rpmLo = (uint8_t)(rpm & 0xFF);
        rpmHi = (uint8_t)(rpm >> 8);
    }
    uint16_t getRPM() const {
        return ((uint16_t)rpmHi << 8) | rpmLo;
    }

    void setMAP(uint16_t map) {
        mapLo = (uint8_t)(map & 0xFF);
        mapHi = (uint8_t)(map >> 8);
    }
    uint16_t getMAP() const {
        return ((uint16_t)mapHi << 8) | mapLo;
    }

    /** PW in µs/100 units (matches Speeduino pw1 field) */
    void setPulseWidth(uint16_t pw) {
        pw1Lo = (uint8_t)(pw & 0xFF);
        pw1Hi = (uint8_t)(pw >> 8);
    }
    uint16_t getPulseWidth() const {
        return ((uint16_t)pw1Hi << 8) | pw1Lo;
    }

    void setRPMDot(int16_t dot) {
        rpmDotLo = (uint8_t)((uint16_t)dot & 0xFF);
        rpmDotHi = (uint8_t)((uint16_t)dot >> 8);
    }

    /** celsius: actual °C; stored as celsius + 40 */
    void setCoolantTemp(int16_t celsius) {
        clt = (uint8_t)(celsius + 40);
    }
    int16_t getCoolantTemp() const { return (int16_t)clt - 40; }

    void setIntakeTemp(int16_t celsius) {
        iat = (uint8_t)(celsius + 40);
    }
    int16_t getIntakeTemp() const { return (int16_t)iat - 40; }

    void setDwell(uint16_t d) {
        dwellLo       = (uint8_t)(d & 0xFF);
        dwellHi       = (uint8_t)(d >> 8);
        actualDwellLo = dwellLo;
        actualDwellHi = dwellHi;
    }

    void setCorrections(uint16_t gammaE) {
        correctionsLo = (uint8_t)(gammaE & 0xFF);
        correctionsHi = (uint8_t)(gammaE >> 8);
    }

    void setTpsDot(int16_t dot) {
        tpsDotLo = (uint8_t)((uint16_t)dot & 0xFF);
        tpsDotHi = (uint8_t)((uint16_t)dot >> 8);
    }
};

#pragma pack(pop)

static_assert(sizeof(EngineStatus) == 130, "EngineStatus must be exactly 130 bytes");

#endif // ENGINE_STATUS_H
