/**
 * @file EngineStatus.h
 * @brief Speeduino real-time data structure (Page 0 / 'A' command response)
 * 
 * This structure matches the Speeduino firmware's real-time data format
 * for serial communication. Total size: 75 bytes + 4 CAN = 79 bytes.
 * 
 * Format version: 0.4 (Speeduino 202310+)
 * Compatibility: TunerStudio, SpeedyLoader, custom loggers
 * 
 * IMPORTANT: Structure must be packed to ensure binary compatibility
 * across different platforms (Arduino AVR, ESP32, ESP8266).
 */

#ifndef ENGINE_STATUS_H
#define ENGINE_STATUS_H

#include <stdint.h>

// Ensure structure is packed (no padding between fields)
#pragma pack(push, 1)

/**
 * @struct EngineStatus
 * @brief Real-time engine parameters (75 bytes + 4 CAN bytes = 79 total)
 * 
 * All multi-byte values use little-endian format (low byte first).
 * Temperatures in °C + 40 (to allow negative values).
 * RPM = (rpmhi * 256 + rpmlo)
 * MAP = maphi * 256 + maplo (kPa)
 * Pulse Width = pw1hi * 256 + pw1lo (0.1ms units)
 */
struct EngineStatus {
    // Byte 0: Command echo
    uint8_t response;           ///< Command response indicator ('A' for realtime data)
    
    // Byte 1: Timestamp
    uint8_t secl;               ///< Seconds counter (0-255, wraps)
    
    // Byte 2-3: Status flags
    uint8_t status1;            ///< General status flags (bitfield)
    uint8_t engine;             ///< Engine status flags (bitfield)
    
    // Byte 4: Ignition
    uint8_t dwell;              ///< Ignition dwell time (0.1ms units)
    
    // Byte 5-6: Manifold pressure
    uint8_t maplo;              ///< MAP low byte (kPa)
    uint8_t maphi;              ///< MAP high byte (kPa)
    
    // Byte 7-8: Temperatures
    uint8_t iat;                ///< Intake air temp (°C + 40)
    uint8_t clt;                ///< Coolant temp (°C + 40)
    
    // Byte 9-10: Battery
    uint8_t batcorrection;      ///< Battery voltage correction (%)
    uint8_t batteryv;           ///< Battery voltage (0.1V units)
    
    // Byte 11-14: Oxygen sensors & corrections
    uint8_t o2;                 ///< Primary O2 sensor (0-255 = lambda 0.5-1.5)
    uint8_t egocorrection;      ///< EGO correction (% above/below 100)
    uint8_t iatcorrection;      ///< IAT correction (% above/below 100)
    uint8_t wue;                ///< Warm-up enrichment (%)
    
    // Byte 15-16: RPM
    uint8_t rpmlo;              ///< RPM low byte
    uint8_t rpmhi;              ///< RPM high byte (RPM = rpmhi*256 + rpmlo)
    
    // Byte 17-20: Fuel calculations
    uint8_t taeamount;          ///< Accel enrichment (% above 100)
    uint8_t gammae;             ///< Total correction (%)
    uint8_t ve;                 ///< Volumetric efficiency (%)
    uint8_t afrtarget;          ///< Target AFR (0.1 AFR units)
    
    // Byte 21-22: Pulse width
    uint8_t pw1lo;              ///< Pulse width low byte (0.1ms units)
    uint8_t pw1hi;              ///< Pulse width high byte
    
    // Byte 23-25: Throttle
    uint8_t tpsdot;             ///< TPS rate of change (%/s)
    uint8_t advance;            ///< Ignition advance (degrees BTDC)
    uint8_t tps;                ///< Throttle position (0-100%)
    
    // Byte 26-27: Performance counters
    uint8_t loopslo;            ///< Loop counter low byte
    uint8_t loopshi;            ///< Loop counter high byte
    
    // Byte 28-29: Memory
    uint8_t freeramlo;          ///< Free RAM low byte
    uint8_t freeramhi;          ///< Free RAM high byte
    
    // Byte 30-31: Boost control
    uint8_t boosttarget;        ///< Boost target (kPa)
    uint8_t boostduty;          ///< Boost duty cycle (%)
    
    // Byte 32: Spark flags
    uint8_t spark;              ///< Spark flags (bitfield)
    
    // Byte 33-34: RPM acceleration
    uint8_t rpmdotlo;           ///< RPM rate of change low byte
    uint8_t rpmdothi;           ///< RPM rate of change high byte
    
    // Byte 35-38: Flex fuel
    uint8_t ethanolpct;         ///< Ethanol percentage (0-100%)
    uint8_t flexcorrection;     ///< Flex fuel correction (%)
    uint8_t flexigncorrection;  ///< Flex ignition correction (degrees)
    uint8_t idleload;           ///< Idle load (%)
    
    // Byte 39: Test outputs
    uint8_t testoutputs;        ///< Test output flags (bitfield)
    
    // Byte 40-41: Additional sensors
    uint8_t o2_2;               ///< Secondary O2 sensor
    uint8_t baro;               ///< Barometric pressure (kPa)
    
    // Byte 42-73: CAN bus data (32 bytes)
    uint8_t canin[32];          ///< CAN input data
    
    // Byte 74-75: Additional sensors
    uint8_t tpsadc;             ///< TPS ADC raw value
    uint8_t errors;             ///< Error code
    
    // Byte 76-78: Unused/reserved (if needed for alignment)
    uint8_t unused1;
    uint8_t unused2;
    uint8_t unused3;
    
    // Helper methods
    void setRPM(uint16_t rpm) {
        rpmlo = rpm & 0xFF;
        rpmhi = (rpm >> 8) & 0xFF;
    }
    
    uint16_t getRPM() const {
        return (static_cast<uint16_t>(rpmhi) << 8) | rpmlo;
    }
    
    void setMAP(uint16_t map) {
        maplo = map & 0xFF;
        maphi = (map >> 8) & 0xFF;
    }
    
    uint16_t getMAP() const {
        return (static_cast<uint16_t>(maphi) << 8) | maplo;
    }
    
    void setPulseWidth(uint16_t pw) {
        pw1lo = pw & 0xFF;
        pw1hi = (pw >> 8) & 0xFF;
    }
    
    uint16_t getPulseWidth() const {
        return (static_cast<uint16_t>(pw1hi) << 8) | pw1lo;
    }
    
    void setRPMDot(int16_t rpmdot) {
        rpmdotlo = rpmdot & 0xFF;
        rpmdothi = (rpmdot >> 8) & 0xFF;
    }
    
    int16_t getRPMDot() const {
        return (static_cast<int16_t>(rpmdothi) << 8) | rpmdotlo;
    }
    
    // Temperature conversion helpers
    void setCoolantTemp(int8_t celsius) {
        clt = static_cast<uint8_t>(celsius + 40);
    }
    
    int8_t getCoolantTemp() const {
        return static_cast<int8_t>(clt) - 40;
    }
    
    void setIntakeTemp(int8_t celsius) {
        iat = static_cast<uint8_t>(celsius + 40);
    }
    
    int8_t getIntakeTemp() const {
        return static_cast<int8_t>(iat) - 40;
    }
};

#pragma pack(pop)

// Compile-time size verification
static_assert(sizeof(EngineStatus) == 79, "EngineStatus must be exactly 79 bytes");

#endif // ENGINE_STATUS_H
