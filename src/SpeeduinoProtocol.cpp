/**
 * @file SpeeduinoProtocol.cpp
 * @brief Implementation of Speeduino serial protocol
 */

#include "SpeeduinoProtocol.h"
#include <string.h>

SpeeduinoProtocol::SpeeduinoProtocol(ISerialInterface* serial, EngineSimulator* simulator)
    : serial(serial)
    , simulator(simulator)
    , commandCount(0)
    , errorCount(0)
    , lastCommandTime(0)
{
}

void SpeeduinoProtocol::begin() {
    serial->begin(SERIAL_BAUD_RATE);
    commandCount = 0;
    errorCount = 0;
}

bool SpeeduinoProtocol::processCommands() {
    if (serial->available() <= 0) {
        return false;
    }
    
    // Read command byte
    int cmdByte = serial->read();
    if (cmdByte < 0) {
        return false;
    }
    
    char command = (char)cmdByte;
    commandCount++;
    
    // Dispatch to appropriate handler
    switch (command) {
        case 'A':  // Real-time data (most common)
            handleRealtimeData();
            break;
            
        case 'Q':  // Status request
            handleStatusRequest();
            break;
            
        case 'V':  // Version request (capital V)
        case 'v':  // Also accept lowercase
            handleVersionRequest();
            break;
            
        case 'S':  // Signature request
            handleSignatureRequest();
            break;
            
        case 'n':  // Page sizes
            handlePageSizesRequest();
            break;
            
        // Additional commands can be added here:
        // case 'B': handleBurnCommand(); break;
        // case 'C': handleTestOutputs(); break;
        // case 'E': handleErrorCodes(); break;
        // case 'F': handleGetPage(); break;
        // etc.
        
        default:
            handleUnknownCommand(command);
            errorCount++;
            break;
    }
    
    return true;
}

void SpeeduinoProtocol::handleRealtimeData() {
    // Get current engine status from simulator
    const EngineStatus& status = simulator->getStatus();
    
    // Send entire structure as binary data
    sendResponse((const uint8_t*)&status, sizeof(EngineStatus));
}

void SpeeduinoProtocol::handleStatusRequest() {
    /**
     * 'Q' command response format:
     * Byte 0: Signature byte (0x00 = "speeduino")
     * Byte 1: Status flags
     * Byte 2: Number of pages
     * Byte 3: Reserved
     * 
     * This is a simplified implementation.
     * Real Speeduino sends more detailed status.
     */
    
    uint8_t response[4];
    response[0] = 0x00;  // Signature byte
    response[1] = 0x01;  // Status: running
    response[2] = 0x01;  // Number of config pages (simplified)
    response[3] = 0x00;  // Reserved
    
    sendResponse(response, 4);
}

void SpeeduinoProtocol::handleVersionRequest() {
    /**
     * 'V' command response:
     * Returns firmware version string followed by newline
     * Format: "speeduino YYYYMM.version"
     * Example: "speeduino 202310.2.0"
     */
    
    const char* version = "speeduino 202310-sim " FIRMWARE_VERSION "\n";
    sendString(version);
}

void SpeeduinoProtocol::handleSignatureRequest() {
    /**
     * 'S' command response:
     * Returns ECU signature for identification
     * Format: 20 bytes signature + version info
     * 
     * This helps TunerStudio identify the ECU type
     */
    
    uint8_t signature[20];
    memset(signature, 0, 20);
    
    // Copy signature string (max 20 bytes)
    const char* sigStr = SPEEDUINO_SIGNATURE;
    size_t len = strlen(sigStr);
    if (len > 20) len = 20;
    memcpy(signature, sigStr, len);
    
    sendResponse(signature, 20);
}

void SpeeduinoProtocol::handlePageSizesRequest() {
    /**
     * 'n' command response:
     * Returns the number and sizes of configuration pages
     * 
     * Format:
     * Byte 0: Number of pages
     * Bytes 1-2: Size of page 0 (little-endian)
     * Bytes 3-4: Size of page 1 (little-endian)
     * etc.
     * 
     * For simulator, we report minimal pages
     */
    
    uint8_t response[7];
    response[0] = 2;         // Number of pages
    
    // Page 0: Settings page (288 bytes in real Speeduino)
    response[1] = 32;        // Low byte (simplified to 32 bytes)
    response[2] = 0;         // High byte
    
    // Page 1: Tuning page (256 bytes in real Speeduino)
    response[3] = 0;         // Low byte (0 = 256)
    response[4] = 1;         // High byte
    
    // Page 2: Reserved
    response[5] = 0;
    response[6] = 0;
    
    sendResponse(response, 7);
}

void SpeeduinoProtocol::handleUnknownCommand(char cmd) {
    /**
     * For unknown commands, send a simple error response
     * In real Speeduino, this might be ignored or return specific error codes
     */
    
    #ifdef DEBUG
        DEBUG_PRINT("Unknown command: 0x");
        DEBUG_PRINTLN(cmd, HEX);
    #endif
    
    // Send error indicator (optional)
    uint8_t error = 0xFF;
    sendResponse(&error, 1);
}

void SpeeduinoProtocol::sendResponse(const uint8_t* data, size_t length) {
    serial->write(data, length);
    serial->flush();
}

void SpeeduinoProtocol::sendString(const char* str) {
    size_t len = strlen(str);
    serial->write((const uint8_t*)str, len);
    serial->flush();
}
