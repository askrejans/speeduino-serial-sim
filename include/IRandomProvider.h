/**
 * @file IRandomProvider.h
 * @brief Hardware abstraction interface for random number generation
 * 
 * Enables both realistic sensor noise in simulation and deterministic
 * testing with seeded random sequences.
 */

#ifndef I_RANDOM_PROVIDER_H
#define I_RANDOM_PROVIDER_H

#include <stdint.h>

/**
 * @interface IRandomProvider
 * @brief Abstract interface for random number generation
 * 
 * Implementations:
 * - ArduinoRandomProvider: Uses Arduino random()
 * - MockRandomProvider: Seeded PRNG for testing
 */
class IRandomProvider {
public:
    virtual ~IRandomProvider() {}
    
    /**
     * @brief Initialize random number generator with seed
     * @param seed Seed value for deterministic sequences
     */
    virtual void seed(uint32_t seed) = 0;
    
    /**
     * @brief Generate random number in range [min, max)
     * @param min Minimum value (inclusive)
     * @param max Maximum value (exclusive)
     * @return Random number in specified range
     */
    virtual int32_t random(int32_t min, int32_t max) = 0;
    
    /**
     * @brief Generate random number in range [0, max)
     * @param max Maximum value (exclusive)
     * @return Random number in range [0, max)
     */
    virtual int32_t random(int32_t max) = 0;
};

#endif // I_RANDOM_PROVIDER_H
