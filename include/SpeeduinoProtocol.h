/**
 * @file SpeeduinoProtocol.h
 * @brief Speeduino serial protocol handler
 * 
 * Implements the Speeduino ECU serial protocol for communication with
 * TunerStudio, SpeedyLoader, and other compatible software.
 * 
 * Supported commands:
 * - 'A': Get real-time data (75 bytes + 4 CAN)
 * - 'Q': ECU status and capabilities
 * - 'V': Firmware version string
 * - 'S': ECU signature (identification)
 * - 'n': Get page sizes
 */

#ifndef SPEEDUINO_PROTOCOL_H
#define SPEEDUINO_PROTOCOL_H

#include "EngineStatus.h"
#include "EngineSimulator.h"
#include "ISerialInterface.h"
#include "Config.h"

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
    /**
     * @brief Constructor
     * @param serial Serial interface for communication
     * @param simulator Engine simulator providing data
     */
    SpeeduinoProtocol(ISerialInterface* serial, EngineSimulator* simulator);
    
    /**
     * @brief Initialize protocol handler
     */
    void begin();
    
    /**
     * @brief Process incoming serial commands (call in loop)
     * @return true if command was processed, false if none available
     */
    bool processCommands();
    
    /**
     * @brief Get total commands processed
     * @return Command count
     */
    uint32_t getCommandCount() const { return commandCount; }
    
    /**
     * @brief Get error count
     * @return Number of invalid commands
     */
    uint32_t getErrorCount() const { return errorCount; }
    
private:
    // Command handlers
    void handleRealtimeData();      // 'A' command
    void handleStatusRequest();     // 'Q' command
    void handleVersionRequest();    // 'V' command (alias 'v')
    void handleSignatureRequest();  // 'S' command
    void handlePageSizesRequest();  // 'n' command
    void handleUnknownCommand(char cmd);
    
    // Utility functions
    void sendResponse(const uint8_t* data, size_t length);
    void sendString(const char* str);
};

#endif // SPEEDUINO_PROTOCOL_H
