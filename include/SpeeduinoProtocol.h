/**
 * @file SpeeduinoProtocol.h
 * @brief Speeduino 202501 serial protocol: legacy single-byte commands and
 *        the framed (msEnvelope) protocol TunerStudio uses.
 *
 * Responses mirror speeduino/comms.cpp and comms_legacy.cpp of release
 * 202501.6. Framed frames are [u16 BE length][payload][u32 BE CRC32];
 * responses are streamed with a running CRC, so no response buffer is needed
 * (an Uno has 2 KB of RAM).
 */
#pragma once

#include <stdint.h>
#include <stddef.h>
#include "Config.h"
#include "EngineSimulator.h"
#include "ISerialInterface.h"
#include "ITimeProvider.h"

namespace rc {
constexpr uint8_t OK = 0x00, BURN_OK = 0x04, TIMEOUT = 0x80, CRC_ERR = 0x82, UNKNOWN = 0x83, RANGE_ERR = 0x84;
}

/// Tune pages TunerStudio reads and writes (sizes from speeduino.ini).
class TunePages {
public:
    static constexpr uint8_t COUNT = 15;
    static uint16_t size(uint8_t page);  ///< page is 1 … 15; 0 if unknown
    uint8_t read(uint8_t page, uint16_t offset) const;
    void write(uint8_t page, uint16_t offset, uint8_t value);
    uint32_t crc(uint8_t page) const;
    static bool stored();                ///< false on AVR: reads are zeros, writes are dropped
private:
#if SIM_DETAILED
    uint8_t data[3616] = {};
    static uint16_t base(uint8_t page);
#endif
};

struct ProtocolStats {
    uint32_t commands = 0;
    uint32_t framed = 0;
    uint32_t realtimeReads = 0;   ///< 'A' and 'r' 0x30
    uint32_t crcErrors = 0;
    uint32_t unknown = 0;
    uint32_t timeouts = 0;
    uint32_t lastCommandMs = 0;
    char lastCommand = 0;
};

class SpeeduinoProtocol {
public:
    SpeeduinoProtocol(ISerialInterface* serial, EngineSimulator* sim, ITimeProvider* time);
    void begin();
    /// Handle at most one command. Returns true if one was processed.
    bool poll();
    const ProtocolStats& stats() const { return stats_; }
    /// Hold each reply until it could have crossed a 115200-baud UART (USB CDC
    /// and TCP are much faster than the real board). On by default where the
    /// line is not a real UART.
    bool wirePacing = SIM_DETAILED != 0;

    static uint32_t crc32Update(uint32_t crc, uint8_t byte);
    static uint32_t crc32(const uint8_t* data, size_t length);

private:
    ISerialInterface* serial;
    EngineSimulator* sim;
    ITimeProvider* time;
    TunePages pages;
    ProtocolStats stats_;
    uint8_t payload[FRAMED_PAYLOAD_MAX];

    // Framed protocol
    void handleFramed(uint16_t length);
    void frameBegin(uint16_t length);
    void frameByte(uint8_t b);
    void frameBytes(const uint8_t* b, uint16_t n);
    void frameString(const char* s);
    void frameEnd();
    void frameCode(uint8_t code);
    uint32_t txCrc = 0;

    // Legacy protocol
    void handleLegacy(char cmd);
    void sendRaw(const uint8_t* b, size_t n);
    void sendRawString(const char* s);

    void sendRealtime(uint16_t offset, uint16_t length, bool framed);
    bool readExact(uint8_t* buf, uint16_t count, uint32_t timeoutMs);
    int readByte(uint32_t timeoutMs);
    void signatureSent();
    void pace(uint32_t bytes);
    uint32_t requestUs = 0;
};
