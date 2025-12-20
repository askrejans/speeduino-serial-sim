/**
 * @file ISerialInterface.h
 * @brief Hardware abstraction interface for serial communication
 * 
 * Allows platform-independent code to communicate via serial port
 * without direct dependencies on Arduino, ESP32, or ESP8266 hardware.
 */

#ifndef I_SERIAL_INTERFACE_H
#define I_SERIAL_INTERFACE_H

#include <stdint.h>
#include <stddef.h>

/**
 * @interface ISerialInterface
 * @brief Abstract interface for serial communication
 * 
 * Implementations:
 * - ArduinoSerialAdapter: Uses Arduino HardwareSerial
 * - ESP32SerialAdapter: Uses ESP32 UART
 * - ESP8266SerialAdapter: Uses ESP8266 UART
 * - MockSerialAdapter: For unit testing
 */
class ISerialInterface {
public:
    virtual ~ISerialInterface() {}
    
    /**
     * @brief Initialize serial communication
     * @param baudRate Communication speed in bits per second
     */
    virtual void begin(uint32_t baudRate) = 0;
    
    /**
     * @brief Check if serial connection is ready
     * @return true if ready, false otherwise
     */
    virtual bool isReady() = 0;
    
    /**
     * @brief Get number of bytes available to read
     * @return Number of bytes in receive buffer
     */
    virtual int available() = 0;
    
    /**
     * @brief Read a single byte from serial port
     * @return Byte read, or -1 if none available
     */
    virtual int read() = 0;
    
    /**
     * @brief Read multiple bytes from serial port
     * @param buffer Destination buffer
     * @param length Maximum bytes to read
     * @return Number of bytes actually read
     */
    virtual size_t readBytes(uint8_t* buffer, size_t length) = 0;
    
    /**
     * @brief Write a single byte to serial port
     * @param byte Byte to write
     * @return Number of bytes written (0 or 1)
     */
    virtual size_t write(uint8_t byte) = 0;
    
    /**
     * @brief Write multiple bytes to serial port
     * @param buffer Source buffer
     * @param length Number of bytes to write
     * @return Number of bytes actually written
     */
    virtual size_t write(const uint8_t* buffer, size_t length) = 0;
    
    /**
     * @brief Flush output buffer (wait for transmission complete)
     */
    virtual void flush() = 0;
    
    /**
     * @brief Clear input buffer
     */
    virtual void clear() = 0;
};

#endif // I_SERIAL_INTERFACE_H
