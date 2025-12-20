/**
 * @file ITimeProvider.h
 * @brief Hardware abstraction interface for timing functions
 * 
 * Decouples time-dependent code from Arduino millis() function,
 * enabling deterministic testing and cross-platform compatibility.
 */

#ifndef I_TIME_PROVIDER_H
#define I_TIME_PROVIDER_H

#include <stdint.h>

/**
 * @interface ITimeProvider
 * @brief Abstract interface for timing operations
 * 
 * Implementations:
 * - ArduinoTimeProvider: Uses Arduino millis()/micros()
 * - MockTimeProvider: For deterministic unit testing
 */
class ITimeProvider {
public:
    virtual ~ITimeProvider() {}
    
    /**
     * @brief Get milliseconds since system start
     * @return Milliseconds elapsed
     */
    virtual uint32_t millis() = 0;
    
    /**
     * @brief Get microseconds since system start
     * @return Microseconds elapsed
     */
    virtual uint32_t micros() = 0;
    
    /**
     * @brief Delay execution for specified milliseconds
     * @param ms Milliseconds to delay
     */
    virtual void delay(uint32_t ms) = 0;
    
    /**
     * @brief Delay execution for specified microseconds
     * @param us Microseconds to delay
     */
    virtual void delayMicroseconds(uint32_t us) = 0;
};

#endif // I_TIME_PROVIDER_H
