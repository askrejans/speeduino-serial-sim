/**
 * @file SpeeduinoProtocol.cpp
 * @brief Speeduino serial protocol – legacy and framed (v2) implementation
 *
 * Framed protocol (used by TunerStudio):
 *   Request:  [2B BE length][payload][4B BE CRC32]
 *   Response: [2B BE length][payload][4B BE CRC32]
 *
 * Legacy protocol (single ASCII byte, e.g. nc testing):
 *   Request:  [1B command]
 *   Response: raw bytes, no framing
 *
 * TunerStudio connection sequence:
 *   'F' (legacy) → {0x00,'0','0','2'}   proto version → TS switches to framed
 *   'C' (framed) → {0x00, 0xFF}          comms test OK
 *   'Q' (framed) → {0x00, version...}    firmware version
 *   'S' (framed) → {0x00, product...}    product string (ECU identity)
 *   'r' (framed) → {0x00, live data...}  output channels
 */

#include "SpeeduinoProtocol.h"
#include <string.h>

// When WiFi serial is active, hardware Serial is free for monitor logging
#if defined(ENABLE_WIFI) && defined(ENABLE_WIFI_SERIAL)
  #include <Arduino.h>
  #define MONITOR_LOG(msg)       Serial.print(msg)
  #define MONITOR_LOGLN(msg)     Serial.println(msg)
  #define MONITOR_LOGF(fmt, ...) Serial.printf(fmt, ##__VA_ARGS__)
#else
  #define MONITOR_LOG(msg)
  #define MONITOR_LOGLN(msg)
  #define MONITOR_LOGF(fmt, ...)
#endif

// Speeduino protocol return codes
static constexpr uint8_t SERIAL_RC_OK       = 0x00U;
static constexpr uint8_t SERIAL_RC_BURN_OK  = 0x04U;
static constexpr uint8_t SERIAL_RC_TIMEOUT  = 0x80U;
static constexpr uint8_t SERIAL_RC_CRC_ERR  = 0x82U;
static constexpr uint8_t SERIAL_RC_UKWN_ERR = 0x83U;

// Sub-command for 'r' output channels request
static constexpr uint8_t CMD_SEND_OUTPUT_CHANNELS = 0x30U; // 48

// ============================================================
// Constructor / begin
// ============================================================

SpeeduinoProtocol::SpeeduinoProtocol(ISerialInterface* serial, EngineSimulator* simulator)
    : serial(serial), simulator(simulator)
    , commandCount(0), errorCount(0), lastCommandTime(0)
{}

void SpeeduinoProtocol::begin() {
    serial->begin(SERIAL_BAUD_RATE);
    commandCount = 0;
    errorCount   = 0;
}

// ============================================================
// Main dispatch – called every loop()
// ============================================================

bool SpeeduinoProtocol::processCommands() {
    if (serial->available() <= 0) return false;

    int firstByte = serial->read();
    if (firstByte < 0) return false;

    commandCount++;

    // DTR byte sent by Windows on connect – silently ignore
    if (firstByte == 0xF0) return true;

    // Detect protocol: printable ASCII letter → legacy single-byte command
    bool isLegacy = (firstByte >= 0x41 && firstByte <= 0x7A) || firstByte == '?';

    if (isLegacy) {
        char cmd = (char)firstByte;
        MONITOR_LOGF("[CMD-L] '%c' (0x%02X) #%u\n",
                     (cmd >= 0x20 && cmd < 0x7F) ? cmd : '.', (uint8_t)cmd, commandCount);
        processLegacyCommand(cmd);
        return true;
    }

    // ---- Framed protocol ----
    // firstByte is the high byte of the 2-byte payload length
    uint8_t lenHi = (uint8_t)firstByte;
    uint8_t lenLo = 0;
    if (!readBytes(&lenLo, 1)) {
        MONITOR_LOGLN("[FRAMED] timeout reading len LSB");
        return true;
    }

    uint16_t payloadLength = ((uint16_t)lenHi << 8) | lenLo;
    if (payloadLength == 0 || payloadLength > MAX_PAYLOAD_SIZE) {
        MONITOR_LOGF("[FRAMED] bad length %u\n", payloadLength);
        errorCount++;
        return true;
    }

    // Read payload
    uint8_t payload[MAX_PAYLOAD_SIZE];
    if (!readBytes(payload, payloadLength, 400)) {
        MONITOR_LOGF("[FRAMED] timeout reading %u payload bytes\n", payloadLength);
        return true;
    }

    // Read 4-byte CRC (big-endian)
    uint8_t crcBuf[4];
    if (!readBytes(crcBuf, 4, 400)) {
        MONITOR_LOGLN("[FRAMED] timeout reading CRC");
        return true;
    }
    uint32_t rxCRC = ((uint32_t)crcBuf[0] << 24) | ((uint32_t)crcBuf[1] << 16)
                   | ((uint32_t)crcBuf[2] << 8)  |  (uint32_t)crcBuf[3];

    // Verify CRC
    uint32_t calcCRC = crc32(payload, payloadLength);
    if (rxCRC != calcCRC) {
        MONITOR_LOGF("[FRAMED] CRC mismatch rx=0x%08X calc=0x%08X\n", rxCRC, calcCRC);
        sendFramedError(SERIAL_RC_CRC_ERR);
        errorCount++;
        return true;
    }

    char cmd = (char)payload[0];
    MONITOR_LOGF("[CMD-F] '%c' (0x%02X) len=%u #%u\n",
                 (cmd >= 0x20 && cmd < 0x7F) ? cmd : '.', (uint8_t)cmd,
                 payloadLength, commandCount);

    processFramedCommand(payload, payloadLength);
    return true;
}

// ============================================================
// Framed command handlers
// ============================================================

void SpeeduinoProtocol::processFramedCommand(const uint8_t* payload, uint16_t length) {
    const EngineStatus& status = simulator->getStatus();

    switch ((char)payload[0]) {

        case 'C': {
            // TunerStudio comms test – must respond {RC_OK, 0xFF}
            uint8_t rsp[] = {SERIAL_RC_OK, 0xFF};
            sendFramedResponse(rsp, 2);
            break;
        }

        case 'Q': {
            // Firmware version / code version string
            // Format: {RC_OK, chars...}  (no null terminator, no padding)
            const char ver[] = "speeduino 202501";
            uint8_t rsp[1 + sizeof(ver) - 1];
            rsp[0] = SERIAL_RC_OK;
            memcpy(&rsp[1], ver, sizeof(ver) - 1);
            sendFramedResponse(rsp, sizeof(rsp));
            break;
        }

        case 'S': {
            // Product / signature string used by TS for ECU identification
            const char prod[] = "Speeduino 202501";
            uint8_t rsp[1 + sizeof(prod) - 1];
            rsp[0] = SERIAL_RC_OK;
            memcpy(&rsp[1], prod, sizeof(prod) - 1);
            sendFramedResponse(rsp, sizeof(rsp));
            // TS3 requires secl reset here
            break;
        }

        case 'F': {
            // Serial protocol version – "002" means framed CRC protocol
            uint8_t rsp[] = {SERIAL_RC_OK, '0', '0', '2'};
            sendFramedResponse(rsp, 4);
            break;
        }

        case 'f': {
            // Serial capability struct: proto_ver, blocking_factor(2B), table_bf(2B)
            uint8_t rsp[] = {SERIAL_RC_OK,
                             0x02,          // protocol version 2
                             0x01, 0x00,    // BLOCKING_FACTOR = 256
                             0x00, 0x40};   // TABLE_BLOCKING_FACTOR = 64
            sendFramedResponse(rsp, sizeof(rsp));
            break;
        }

        case 'A': {
            // Legacy-style realtime data via framed protocol
            // Response: RC_OK + full EngineStatus (130 bytes)
            uint8_t rsp[1 + sizeof(EngineStatus)];
            rsp[0] = SERIAL_RC_OK;
            memcpy(&rsp[1], &status, sizeof(EngineStatus));
            sendFramedResponse(rsp, sizeof(rsp));
            break;
        }

        case 'r': {
            // Optimised OutputChannels read
            // payload: [0]='r' [1]=ignored [2]=sub-cmd [3-4]=offset LE [5-6]=len LE
            if (length < 7) { sendFramedError(SERIAL_RC_UKWN_ERR); break; }

            uint8_t  subcmd = payload[2];
            uint16_t offset = (uint16_t)payload[3] | ((uint16_t)payload[4] << 8);
            uint16_t reqLen = (uint16_t)payload[5] | ((uint16_t)payload[6] << 8);

            if (subcmd == CMD_SEND_OUTPUT_CHANNELS) {
                const uint8_t* raw = (const uint8_t*)&status;
                uint16_t available = sizeof(EngineStatus);
                if (reqLen > MAX_PAYLOAD_SIZE - 1) reqLen = MAX_PAYLOAD_SIZE - 1;

                uint8_t rsp[1 + reqLen];
                rsp[0] = SERIAL_RC_OK;
                for (uint16_t i = 0; i < reqLen; i++) {
                    uint16_t idx = offset + i;
                    rsp[1 + i] = (idx < available) ? raw[idx] : 0x00;
                }
                MONITOR_LOGF("[RSP-r] offset=%u len=%u\n", offset, reqLen);
                sendFramedResponse(rsp, 1 + reqLen);
            } else if (subcmd == 0x0F) {
                // Signature sub-request
                const char ver[] = "speeduino 202501";
                uint8_t rsp[1 + sizeof(ver) - 1];
                rsp[0] = SERIAL_RC_OK;
                memcpy(&rsp[1], ver, sizeof(ver) - 1);
                sendFramedResponse(rsp, sizeof(rsp));
            } else {
                sendFramedError(SERIAL_RC_UKWN_ERR);
            }
            break;
        }

        case 'p': {
            // Read tune page
            // payload: [0]='p' [1]=ignored [2]=page [3-4]=offset LE [5-6]=len LE
            if (length < 7) { sendFramedError(SERIAL_RC_UKWN_ERR); break; }
            uint16_t reqLen = (uint16_t)payload[5] | ((uint16_t)payload[6] << 8);
            if (reqLen > MAX_PAYLOAD_SIZE - 1) reqLen = MAX_PAYLOAD_SIZE - 1;

            uint8_t rsp[1 + reqLen];
            rsp[0] = SERIAL_RC_OK;
            memset(&rsp[1], 0x00, reqLen); // Simulator returns zeros for all pages
            sendFramedResponse(rsp, 1 + reqLen);
            break;
        }

        case 'd': {
            // Page CRC32 request – payload[2] = page number
            // Return CRC32 of an all-zero page (simulator has no real tune)
            uint8_t dummyPage[288] = {};
            uint32_t pageCrc = crc32(dummyPage, sizeof(dummyPage));
            pageCrc = ((pageCrc >> 24) & 0xFF) | (((pageCrc >> 16) & 0xFF) << 8)
                    | (((pageCrc >> 8) & 0xFF) << 16) | ((pageCrc & 0xFF) << 24); // BE
            uint8_t rsp[5];
            rsp[0] = SERIAL_RC_OK;
            rsp[1] = (pageCrc >> 24) & 0xFF;
            rsp[2] = (pageCrc >> 16) & 0xFF;
            rsp[3] = (pageCrc >>  8) & 0xFF;
            rsp[4] =  pageCrc        & 0xFF;
            sendFramedResponse(rsp, 5);
            break;
        }

        case 'b':
        case 'B': {
            // Burn to EEPROM – acknowledge, nothing to burn in simulator
            sendFramedError(SERIAL_RC_BURN_OK);
            break;
        }

        case 'n': {
            // Page count and sizes (framed variant)
            // Speeduino typically reports 15 pages; simulator exposes 2
            uint8_t rsp[5] = {SERIAL_RC_OK,
                              2,        // number of pages
                              0x20, 0x01, // page 0: 288 bytes LE
                              0x00};
            sendFramedResponse(rsp, 5);
            break;
        }

        case 'I': {
            // CAN ID – respond with {RC_OK, 0x00}
            uint8_t rsp[] = {SERIAL_RC_OK, 0x00};
            sendFramedResponse(rsp, sizeof(rsp));
            break;
        }

        case 'E': {
            // Command button – just ACK
            sendFramedError(SERIAL_RC_OK);
            break;
        }

        default:
            MONITOR_LOGF("[CMD-F] unknown 0x%02X\n", payload[0]);
            sendFramedError(SERIAL_RC_UKWN_ERR);
            errorCount++;
            break;
    }
}

// ============================================================
// Legacy single-byte command handlers (for nc / SpeedyLoader)
// ============================================================

void SpeeduinoProtocol::processLegacyCommand(char cmd) {
    // Real Speeduino firmware sends RAW (unframed) responses for legacy commands.
    // Legacy mode is used for the initial port-check handshake and tools like nc.
    // TunerStudio switches to framed protocol only after receiving "002" from 'F'.
    const EngineStatus& status = simulator->getStatus();

    switch (cmd) {
        case 'F': {
            // Raw "002" – no RC_OK, no framing. TS uses this to detect the protocol version.
            sendLegacyString("002");
            break;
        }
        case 'Q': {
            // Raw version string – TS compares this directly against signature= in the INI
            sendLegacyString("speeduino 202501");
            break;
        }
        case 'S': {
            // Raw product string displayed in TS as the ECU identity
            sendLegacyString("Speeduino 202501");
            break;
        }
        case 'V':
        case 'v': {
            sendLegacyString("speeduino 202501");
            break;
        }
        case 'C': {
            // Raw comms test – real Speeduino sends byte 0x01
            uint8_t rsp = 0x01;
            sendLegacyResponse(&rsp, 1);
            break;
        }
        case 'r': {
            // Optimised output-channels request in legacy mode.
            // Format: 'r' + [tsCanId] + [subcmd] + [offsetLo] + [offsetHi] + [lenLo] + [lenHi]
            // Must read all 6 follow-up bytes; if left unread they corrupt the framed parser.
            uint8_t extra[6] = {};
            readBytes(extra, 6, 200); // consume; ignore timeout – send whatever we have
            uint8_t  subcmd = extra[1];
            uint16_t offset = (uint16_t)extra[2] | ((uint16_t)extra[3] << 8);
            uint16_t reqLen = (uint16_t)extra[4] | ((uint16_t)extra[5] << 8);
            if (subcmd == CMD_SEND_OUTPUT_CHANNELS) {
                const uint8_t* raw = (const uint8_t*)&status;
                uint16_t available = sizeof(EngineStatus);
                if (reqLen == 0 || reqLen > MAX_PAYLOAD_SIZE) reqLen = available;
                uint8_t rsp[MAX_PAYLOAD_SIZE];
                for (uint16_t i = 0; i < reqLen; i++) {
                    uint16_t idx = offset + i;
                    rsp[i] = (idx < available) ? raw[idx] : 0x00;
                }
                sendLegacyResponse(rsp, reqLen);
            }
            // other sub-commands: no response (TS ignores)
            break;
        }
        case 'A': {
            // Realtime data – raw struct dump (legacy format used by SpeedyLoader etc.)
            sendLegacyResponse((const uint8_t*)&status, sizeof(EngineStatus));
            break;
        }
        case 'X':
        case 'O':
        case 'J':
        case 'H': {
            // Logger start commands – TS expects a single 0x01 ack byte
            uint8_t ack = 0x01;
            sendLegacyResponse(&ack, 1);
            break;
        }
        case 'x':
        case 'o':
        case 'j':
        case 'h': {
            // Logger stop commands – no response expected
            break;
        }
        case 'n': {
            // Page count – raw bytes
            uint8_t rsp[] = {2, 0x20, 0x01, 0x00, 0x01, 0, 0};
            sendLegacyResponse(rsp, sizeof(rsp));
            break;
        }
        default: {
            // Unknown legacy command – ignore silently (matches real Speeduino behaviour)
            break;
        }
    }
}

// ============================================================
// Framed I/O helpers
// ============================================================

void SpeeduinoProtocol::sendFramedResponse(const uint8_t* data, uint16_t length) {
    uint32_t crc = crc32(data, length);

    // 2-byte length (big-endian)
    serial->write((uint8_t)(length >> 8));
    serial->write((uint8_t)(length & 0xFF));
    // Payload
    serial->write(data, length);
    // 4-byte CRC (big-endian)
    serial->write((uint8_t)(crc >> 24));
    serial->write((uint8_t)(crc >> 16));
    serial->write((uint8_t)(crc >> 8));
    serial->write((uint8_t)(crc & 0xFF));
    serial->flush();

    MONITOR_LOGF("[RSP-F] %u bytes\n", length);
}

void SpeeduinoProtocol::sendFramedError(uint8_t errorCode) {
    sendFramedResponse(&errorCode, 1);
}

// ============================================================
// Legacy I/O helpers
// ============================================================

void SpeeduinoProtocol::sendLegacyResponse(const uint8_t* data, size_t length) {
    serial->write(data, length);
    serial->flush();
    MONITOR_LOGF("[RSP-L] %u bytes\n", (unsigned)length);
}

void SpeeduinoProtocol::sendLegacyString(const char* str) {
    size_t len = strlen(str);
    serial->write((const uint8_t*)str, len);
    serial->flush();
    MONITOR_LOGF("[RSP-L] \"%s\"\n", str);
}

// ============================================================
// Compatibility wrappers (used by any remaining legacy callers)
// ============================================================

void SpeeduinoProtocol::sendResponse(const uint8_t* data, size_t length) {
    sendLegacyResponse(data, length);
}

void SpeeduinoProtocol::sendString(const char* str) {
    sendLegacyString(str);
}

// ============================================================
// Blocking byte reader with timeout
// ============================================================

bool SpeeduinoProtocol::readBytes(uint8_t* buf, uint16_t count, uint32_t timeoutMs) {
    uint32_t start = millis();
    uint16_t got = 0;
    while (got < count) {
        if (serial->available() > 0) {
            int b = serial->read();
            if (b >= 0) buf[got++] = (uint8_t)b;
        } else if (millis() - start > timeoutMs) {
            return false;
        }
    }
    return true;
}

// ============================================================
// CRC32 (ISO 3309 / IEEE 802.3 – same as FastCRC32 / zlib crc32)
// ============================================================

uint32_t SpeeduinoProtocol::crc32(const uint8_t* data, size_t length) {
    uint32_t crc = 0xFFFFFFFFUL;
    for (size_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            crc = (crc >> 1) ^ (0xEDB88320UL & -(crc & 1));
        }
    }
    return crc ^ 0xFFFFFFFFUL;
}

