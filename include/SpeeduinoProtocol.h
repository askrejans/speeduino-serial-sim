/**
 * @file SpeeduinoProtocol.h
 * @brief Speeduino serial protocol handler
 *
 * Implements both legacy (single-byte) and framed (v2) Speeduino protocol.
 *
 * Framed protocol format:
 *   Request:  [2-byte BE length] [payload bytes] [4-byte BE CRC32]
 *   Response: [2-byte BE length] [payload bytes] [4-byte BE CRC32]
 *
 * TunerStudio connection sequence:
 *   1. Legacy 'F' → raw response {0x00,'0','0','2'} (protocol version 2)
 *   2. Framed 'C' → {SERIAL_RC_OK, 0xFF}   (comms test)
 *   3. Framed 'Q' → {SERIAL_RC_OK, version string}
 *   4. Framed 'S' → {SERIAL_RC_OK, product string}
 *   5. Framed 'r' → {SERIAL_RC_OK, output channel bytes}
 */

#ifndef SPEEDUINO_PROTOCOL_H
#define SPEEDUINO_PROTOCOL_H

#include "EngineStatus.h"
#include "EngineSimulator.h"
#include "ISerialInterface.h"
#include "Config.h"

// Maximum framed payload we'll accept from TunerStudio
#define MAX_PAYLOAD_SIZE 512

/**
 * @class SpeeduinoProtocol
 * @brief Serial protocol handler for Speeduino commands
 */
class SpeeduinoProtocol {
private:
    ISerialInterface* serial;
    EngineSimulator* simulator;

    // Statistics
    uint32_t commandCount;
    uint32_t errorCount;
    uint32_t lastCommandTime;

public:
    SpeeduinoProtocol(ISerialInterface* serial, EngineSimulator* simulator);
    void begin();

    /**
     * @brief Process one incoming command (call repeatedly in loop)
     * @return true if a command was processed
     */
    bool processCommands();

    uint32_t getCommandCount() const { return commandCount; }
    uint32_t getErrorCount()   const { return errorCount; }

private:
    // ---- Framed protocol (v2) ----
    void processFramedCommand(const uint8_t* payload, uint16_t length);
    void sendFramedResponse(const uint8_t* data, uint16_t length);
    void sendFramedError(uint8_t errorCode);

    // ---- Legacy single-byte protocol ----
    void processLegacyCommand(char cmd);
    void sendLegacyResponse(const uint8_t* data, size_t length);
    void sendLegacyString(const char* str);

    // ---- Shared helpers ----
    void sendResponse(const uint8_t* data, size_t length);  // kept for compat
    void sendString(const char* str);                        // kept for compat

    /**
     * @brief Read exactly `count` bytes with a timeout
     * @return true if all bytes were read before timeout
     */
    bool readBytes(uint8_t* buf, uint16_t count, uint32_t timeoutMs = 200);

    /** @brief Standard CRC32 (Ethernet/ZIP polynomial) */
    static uint32_t crc32(const uint8_t* data, size_t length);
};

#endif // SPEEDUINO_PROTOCOL_H
