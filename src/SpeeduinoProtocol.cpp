/**
 * @file SpeeduinoProtocol.cpp
 * @brief Command handling as in speeduino 202501.6 comms.cpp / comms_legacy.cpp.
 *
 * TunerStudio: legacy 'F' → "002", then framed 'Q', 'S', 'f', page reads
 * ('p', 'd'), and 'r' 0x30 for the 130 output-channel bytes. Gateways such as
 * ecu-to-mqtt use legacy 'Q' and 'A'.
 */
#include "SpeeduinoProtocol.h"
#include <string.h>

namespace {
constexpr uint8_t OUTPUT_CHANNELS = 0x30;
constexpr uint8_t SIGNATURE_REQUEST = 0x0F;
constexpr uint32_t BYTE_TIMEOUT_MS = 500;
// From speeduino.ini (pageSize), pages 1 … 15.
const uint16_t PAGE_SIZES[TunePages::COUNT] = {128, 288, 288, 128, 288, 128, 240, 384,
                                              192, 192, 288, 192, 128, 288, 256};
// comms.h values for AVR and STM32 boards, which the CORE4 reports.
constexpr uint8_t BLOCKING_FACTOR = 121, TABLE_BLOCKING_FACTOR = 64;
}  // namespace

// ---------------------------------------------------------------- pages

uint16_t TunePages::size(uint8_t page) {
    return page >= 1 && page <= COUNT ? PAGE_SIZES[page - 1] : 0;
}

bool TunePages::stored() { return SIM_DETAILED != 0; }

#if SIM_DETAILED
uint16_t TunePages::base(uint8_t page) {
    uint16_t b = 0;
    for (uint8_t p = 1; p < page; p++) b += PAGE_SIZES[p - 1];
    return b;
}
uint8_t TunePages::read(uint8_t page, uint16_t offset) const {
    return offset < size(page) ? data[base(page) + offset] : 0;
}
void TunePages::write(uint8_t page, uint16_t offset, uint8_t value) {
    if (offset < size(page)) data[base(page) + offset] = value;
}
#else
uint8_t TunePages::read(uint8_t, uint16_t) const { return 0; }
void TunePages::write(uint8_t, uint16_t, uint8_t) {}
#endif

uint32_t TunePages::crc(uint8_t page) const {
    uint32_t c = 0xFFFFFFFFUL;
    for (uint16_t i = 0; i < size(page); i++) c = SpeeduinoProtocol::crc32Update(c, read(page, i));
    return c ^ 0xFFFFFFFFUL;
}

// ---------------------------------------------------------------- core

SpeeduinoProtocol::SpeeduinoProtocol(ISerialInterface* s, EngineSimulator* e, ITimeProvider* t)
    : serial(s), sim(e), time(t) {}

void SpeeduinoProtocol::begin() {
    serial->begin(SERIAL_BAUD_RATE);
    stats_ = ProtocolStats();
}

uint32_t SpeeduinoProtocol::crc32Update(uint32_t crc, uint8_t byte) {
    crc ^= byte;
    for (uint8_t j = 0; j < 8; j++) crc = (crc >> 1) ^ (0xEDB88320UL & (0UL - (crc & 1UL)));
    return crc;
}

uint32_t SpeeduinoProtocol::crc32(const uint8_t* data, size_t length) {
    uint32_t c = 0xFFFFFFFFUL;
    for (size_t i = 0; i < length; i++) c = crc32Update(c, data[i]);
    return c ^ 0xFFFFFFFFUL;
}

int SpeeduinoProtocol::readByte(uint32_t timeoutMs) {
    uint32_t start = time->millis();
    while (serial->available() <= 0) {
        if (time->millis() - start > timeoutMs) return -1;
    }
    return serial->read();
}

bool SpeeduinoProtocol::readExact(uint8_t* buf, uint16_t count, uint32_t timeoutMs) {
    for (uint16_t i = 0; i < count; i++) {
        int b = readByte(timeoutMs);
        if (b < 0) return false;
        buf[i] = (uint8_t)b;
    }
    return true;
}

bool SpeeduinoProtocol::poll() {
    if (serial->available() <= 0) return false;
    int first = serial->read();
    if (first < 0) return false;
    stats_.commands++;
    stats_.lastCommandMs = time->millis();
    requestUs = time->micros();

    // A framed request starts with the high byte of a length below 0x100
    // in practice; every legacy command is a printable letter or '?'.
    if ((first >= 'A' && first <= 'z') || first == '?') {
        stats_.lastCommand = (char)first;
        handleLegacy((char)first);
        return true;
    }
    if (first == 0xF0) return true;  // Windows sends this on open

    int lo = readByte(BYTE_TIMEOUT_MS);
    if (lo < 0) {
        stats_.timeouts++;
        return true;
    }
    uint16_t length = (uint16_t)(first << 8 | lo);
    if (length == 0) return true;

    // Stream the payload: keep what fits, CRC everything.
    uint32_t c = 0xFFFFFFFFUL;
    for (uint16_t i = 0; i < length; i++) {
        int b = readByte(BYTE_TIMEOUT_MS);
        if (b < 0) {
            stats_.timeouts++;
            frameCode(rc::TIMEOUT);
            return true;
        }
        c = crc32Update(c, (uint8_t)b);
        if (i < sizeof(payload)) payload[i] = (uint8_t)b;
    }
    uint8_t crcBytes[4];
    if (!readExact(crcBytes, 4, BYTE_TIMEOUT_MS)) {
        stats_.timeouts++;
        frameCode(rc::TIMEOUT);
        return true;
    }
    uint32_t received = (uint32_t)crcBytes[0] << 24 | (uint32_t)crcBytes[1] << 16 | (uint32_t)crcBytes[2] << 8 | crcBytes[3];
    if (received != (c ^ 0xFFFFFFFFUL)) {
        stats_.crcErrors++;
        frameCode(rc::CRC_ERR);
        return true;
    }
    stats_.framed++;
    stats_.lastCommand = (char)payload[0];
    handleFramed(length);
    return true;
}

// ---------------------------------------------------------------- framed

void SpeeduinoProtocol::pace(uint32_t bytes) {
    if (!wirePacing) return;
    // 10 bits per byte at 115200 baud, plus the firmware's ~0.3 ms to start.
    uint32_t due = bytes * 10000000UL / SERIAL_BAUD_RATE + 300;
    uint32_t spent = time->micros() - requestUs;
    if (spent < due) time->delayMicroseconds(due - spent);
}

void SpeeduinoProtocol::frameBegin(uint16_t length) {
    pace(length + 6U);
    serial->write((uint8_t)(length >> 8));
    serial->write((uint8_t)(length & 0xFF));
    txCrc = 0xFFFFFFFFUL;
}
void SpeeduinoProtocol::frameByte(uint8_t b) {
    serial->write(b);
    txCrc = crc32Update(txCrc, b);
}
void SpeeduinoProtocol::frameBytes(const uint8_t* b, uint16_t n) {
    serial->write(b, n);
    for (uint16_t i = 0; i < n; i++) txCrc = crc32Update(txCrc, b[i]);
}
void SpeeduinoProtocol::frameString(const char* s) { frameBytes((const uint8_t*)s, (uint16_t)strlen(s)); }
void SpeeduinoProtocol::frameEnd() {
    uint32_t c = txCrc ^ 0xFFFFFFFFUL;
    serial->write((uint8_t)(c >> 24));
    serial->write((uint8_t)(c >> 16));
    serial->write((uint8_t)(c >> 8));
    serial->write((uint8_t)c);
    serial->flush();
}
void SpeeduinoProtocol::frameCode(uint8_t code) {
    frameBegin(1);
    frameByte(code);
    frameEnd();
}

static uint16_t le16(const uint8_t* p) { return (uint16_t)(p[0] | (uint16_t)p[1] << 8); }

void SpeeduinoProtocol::handleFramed(uint16_t length) {
    const uint8_t* p = payload;
    const uint16_t kept = length < sizeof(payload) ? length : sizeof(payload);
    switch ((char)p[0]) {
        case 'A':
            sendRealtime(0, sizeof(EngineStatus), true);
            break;
        case 'b':
        case 'B':
            frameCode(rc::BURN_OK);
            break;
        case 'C':
            frameBegin(2);
            frameByte(rc::OK);
            frameByte(255);
            frameEnd();
            break;
        case 'd': {
            if (kept < 3) { frameCode(rc::RANGE_ERR); break; }
            uint32_t c = pages.crc(p[2]);
            frameBegin(5);
            frameByte(rc::OK);
            frameByte((uint8_t)(c >> 24));
            frameByte((uint8_t)(c >> 16));
            frameByte((uint8_t)(c >> 8));
            frameByte((uint8_t)c);
            frameEnd();
            break;
        }
        case 'E':
        case 'I':
            frameBegin(2);
            frameByte(rc::OK);
            frameByte(0);  // CAN ID
            frameEnd();
            break;
        case 'f':
            frameBegin(6);
            frameByte(rc::OK);
            frameByte(2);
            frameByte(0);
            frameByte(BLOCKING_FACTOR);
            frameByte(0);
            frameByte(TABLE_BLOCKING_FACTOR);
            frameEnd();
            break;
        case 'F':
            frameBegin(4);
            frameByte(rc::OK);
            frameString(SERIAL_PROTOCOL_VERSION);
            frameEnd();
            break;
        case 'H': case 'h': case 'J': case 'j': case 'O': case 'o': case 'X': case 'x':
            frameCode(rc::OK);  // loggers: nothing to log
            break;
        case 'k':
            frameBegin(5);
            frameByte(rc::OK);
            for (uint8_t i = 0; i < 4; i++) frameByte(0);
            frameEnd();
            break;
        case 'M': {
            // page (2), offset (2), length (2), data …
            if (kept < 7) { frameCode(rc::RANGE_ERR); break; }
            uint8_t page = p[2];
            uint16_t offset = le16(p + 3), n = le16(p + 5);
            if (!TunePages::size(page) || offset + n > TunePages::size(page)) { frameCode(rc::RANGE_ERR); break; }
            for (uint16_t i = 0; i < n && 7 + i < kept; i++) pages.write(page, offset + i, p[7 + i]);
            frameCode(rc::OK);
            break;
        }
        case 'p': {
            if (kept < 7) { frameCode(rc::RANGE_ERR); break; }
            uint8_t page = p[2];
            uint16_t offset = le16(p + 3), n = le16(p + 5);
            frameBegin((uint16_t)(n + 1));
            frameByte(rc::OK);
            for (uint16_t i = 0; i < n; i++) frameByte(pages.read(page, offset + i));
            frameEnd();
            break;
        }
        case 'Q':
            frameBegin((uint16_t)(1 + strlen(SPEEDUINO_SIGNATURE)));
            frameByte(rc::OK);
            frameString(SPEEDUINO_SIGNATURE);
            frameEnd();
            break;
        case 'r': {
            if (kept < 7) { frameCode(rc::RANGE_ERR); break; }
            uint8_t sub = p[2];
            if (sub == OUTPUT_CHANNELS) {
                sendRealtime(le16(p + 3), le16(p + 5), true);
            } else if (sub == SIGNATURE_REQUEST) {
                frameBegin((uint16_t)(1 + strlen(SPEEDUINO_SIGNATURE)));
                frameByte(rc::OK);
                frameString(SPEEDUINO_SIGNATURE);
                frameEnd();
            }
            // Other sub-commands: the firmware sends nothing.
            break;
        }
        case 'S':
            frameBegin((uint16_t)(1 + strlen(SPEEDUINO_PRODUCT)));
            frameByte(rc::OK);
            frameString(SPEEDUINO_PRODUCT);
            frameEnd();
            signatureSent();
            break;
        case 't':
            frameCode(rc::OK);  // calibration upload: accepted, not applied
            break;
        default:
            stats_.unknown++;
            frameCode(rc::UNKNOWN);
            break;
    }
}

void SpeeduinoProtocol::sendRealtime(uint16_t offset, uint16_t length, bool framed) {
    stats_.realtimeReads++;
    const EngineStatus& s = sim->getStatus();
    const uint8_t* raw = (const uint8_t*)&s;
    if (framed) {
        frameBegin((uint16_t)(length + 1));
        frameByte(rc::OK);
        for (uint16_t i = 0; i < length; i++) {
            uint16_t at = offset + i;
            frameByte(at < sizeof(EngineStatus) ? raw[at] : 0);
        }
        frameEnd();
    } else {
        pace(length);
        for (uint16_t i = 0; i < length; i++) {
            uint16_t at = offset + i;
            serial->write(at < sizeof(EngineStatus) ? raw[at] : 0);
        }
        serial->flush();
    }
}

void SpeeduinoProtocol::signatureSent() {
    // comms.cpp: 'S' zeroes secl for TunerStudio and, over the primary port,
    // stops the secondary port from taking TunerStudio comms.
    sim->resetSeconds();
}

// ---------------------------------------------------------------- legacy

void SpeeduinoProtocol::sendRaw(const uint8_t* b, size_t n) {
    pace((uint32_t)n);
    serial->write(b, n);
    serial->flush();
}
void SpeeduinoProtocol::sendRawString(const char* s) { sendRaw((const uint8_t*)s, strlen(s)); }

void SpeeduinoProtocol::handleLegacy(char cmd) {
    const EngineStatus& s = sim->getStatus();
    switch (cmd) {
        case 'A':
            sendRealtime(0, sizeof(EngineStatus), false);
            break;
        case 'C': {
            uint8_t one = 1;
            sendRaw(&one, 1);
            break;
        }
        case 'c': {
            uint8_t b[2] = {s.loopsLo, s.loopsHi};
            sendRaw(b, 2);
            break;
        }
        case 'd': {
            uint8_t args[2];
            if (!readExact(args, 2, BYTE_TIMEOUT_MS)) { stats_.timeouts++; break; }
            uint32_t c = pages.crc(args[1]);
            uint8_t b[4] = {(uint8_t)(c >> 24), (uint8_t)(c >> 16), (uint8_t)(c >> 8), (uint8_t)c};
            sendRaw(b, 4);
            break;
        }
        case 'F':
            sendRawString(SERIAL_PROTOCOL_VERSION);
            break;
        case 'm': {
            uint8_t b[2] = {s.freeRamLo, s.freeRamHi};
            sendRaw(b, 2);
            break;
        }
        case 'Q':
            sendRawString(SPEEDUINO_SIGNATURE);
            break;
        case 'r': {
            // $tsCanId, sub-command, offset (2, LE), length (2, LE)
            uint8_t a[6];
            if (!readExact(a, 6, BYTE_TIMEOUT_MS)) { stats_.timeouts++; break; }
            if (a[1] == OUTPUT_CHANNELS) sendRealtime(le16(a + 2), le16(a + 4), false);
            break;
        }
        case 'S':
            sendRawString(SPEEDUINO_PRODUCT);
            signatureSent();
            break;
        default:
            // The firmware ignores what it does not know (or the command is a
            // write/tooling one this simulator does not implement).
            stats_.unknown++;
            break;
    }
}
