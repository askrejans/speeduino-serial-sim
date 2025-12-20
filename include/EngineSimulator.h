/**
 * @file EngineSimulator.h
 * @brief Realistic inline-4 engine simulation with correlated parameters
 * 
 * Simulates a 2.0L I4 naturally aspirated gasoline engine with realistic:
 * - Thermodynamic relationships (MAP, VE, AFR, timing)
 * - Transient behavior (acceleration, deceleration, warmup)
 * - Sensor noise and dynamics
 * - State machine for different operating modes
 */

#ifndef ENGINE_SIMULATOR_H
#define ENGINE_SIMULATOR_H

#include "EngineStatus.h"
#include "ITimeProvider.h"
#include "IRandomProvider.h"
#include "Config.h"

/**
 * @enum EngineMode
 * @brief Operating modes for engine simulation state machine
 */
enum class EngineMode {
    STARTUP,        ///< Initial cold start (0-2s)
    WARMUP_IDLE,    ///< Warming up at idle (2-30s)
    IDLE,           ///< Normal idle (warm engine)
    LIGHT_LOAD,     ///< Light throttle, cruising
    ACCELERATION,   ///< Moderate to heavy acceleration
    HIGH_RPM,       ///< High RPM operation (>5000 RPM)
    DECELERATION,   ///< Throttle closed, engine braking
    WOT             ///< Wide open throttle (full load)
};

/**
 * @class EngineSimulator
 * @brief Physics-based engine simulation
 * 
 * Features:
 * - Realistic parameter correlations (not random)
 * - Smooth transitions between states
 * - Thermal model (coolant/intake temps)
 * - Volumetric efficiency curves
 * - Fuel delivery calculations
 * - Ignition timing maps
 */
class EngineSimulator {
private:
    ITimeProvider* timeProvider;
    IRandomProvider* randomProvider;
    EngineStatus status;
    
    // Simulation state
    EngineMode currentMode;
    uint32_t lastUpdateTime;
    uint32_t stateStartTime;
    uint32_t engineStartTime;
    
    // Dynamic state variables
    uint16_t targetRPM;
    uint16_t currentRPM;
    int16_t rpmAcceleration;    // RPM/s
    uint8_t targetThrottle;
    uint8_t currentThrottle;
    
    // Thermal state
    int16_t coolantTemp;        // Actual temp in °C * 10
    int16_t intakeTemp;         // Actual temp in °C * 10
    int16_t exhaustTemp;        // For realistic IAT heating
    
    // Fuel state
    uint16_t pulseWidth;        // 0.1ms units
    uint8_t injectorDutyCycle;
    
    // Counters
    uint32_t loopCounter;
    uint16_t secondCounter;
    
public:
    /**
     * @brief Constructor
     * @param timeProvider Time abstraction for millis()
     * @param randomProvider Random number generator for sensor noise
     */
    EngineSimulator(ITimeProvider* timeProvider, IRandomProvider* randomProvider);
    
    /**
     * @brief Initialize engine to cold start state
     */
    void initialize();
    
    /**
     * @brief Update simulation (call at ~20Hz / 50ms intervals)
     * @return true if state changed, false otherwise
     */
    bool update();
    
    /**
     * @brief Get current engine status structure
     * @return Reference to EngineStatus
     */
    const EngineStatus& getStatus() const { return status; }
    
    /**
     * @brief Get current operating mode
     * @return Current EngineMode
     */
    EngineMode getMode() const { return currentMode; }
    
    /**
     * @brief Force mode change (for testing)
     * @param mode Desired mode
     */
    void setMode(EngineMode mode);
    
    /**
     * @brief Get elapsed time since engine start
     * @return Seconds since initialize()
     */
    uint32_t getRuntime() const;
    
private:
    // State machine
    void updateStateMachine();
    void transitionToMode(EngineMode newMode);
    
    // Physics simulation
    void simulateRPM();
    void simulateThermal();
    void simulateMAP();
    void simulateThrottle();
    void simulateFuel();
    void simulateIgnition();
    void simulateAFR();
    void simulateCorrections();
    void simulateSensors();
    void simulateVoltage();
    void simulateCANData();
    
    // Helper functions
    uint8_t calculateVE(uint16_t rpm, uint8_t tps);
    uint8_t calculateIgnitionAdvance(uint16_t rpm, uint8_t load);
    uint16_t calculateRequiredPulseWidth(uint16_t rpm, uint16_t map, uint8_t ve);
    uint8_t getWarmupEnrichment(int16_t coolantTemp);
    int8_t addNoise(int8_t value, int8_t range);
    int16_t interpolate(int16_t current, int16_t target, uint8_t rate);
    uint16_t mapValue(uint16_t x, uint16_t in_min, uint16_t in_max, 
                      uint16_t out_min, uint16_t out_max);
};

#endif // ENGINE_SIMULATOR_H
