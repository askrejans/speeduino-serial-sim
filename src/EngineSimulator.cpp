/**
 * @file EngineSimulator.cpp
 * @brief Implementation of realistic I4 engine simulation
 */

#include "EngineSimulator.h"
#include <string.h>

EngineSimulator::EngineSimulator(ITimeProvider* timeProvider, IRandomProvider* randomProvider)
    : timeProvider(timeProvider)
    , randomProvider(randomProvider)
    , currentMode(EngineMode::STARTUP)
    , lastUpdateTime(0)
    , stateStartTime(0)
    , engineStartTime(0)
    , targetRPM(0)
    , currentRPM(0)
    , rpmAcceleration(0)
    , targetThrottle(0)
    , currentThrottle(0)
    , coolantTemp(TEMP_AMBIENT)
    , intakeTemp(TEMP_AMBIENT)
    , exhaustTemp(TEMP_AMBIENT)
    , pulseWidth(0)
    , injectorDutyCycle(0)
    , loopCounter(0)
    , secondCounter(0)
{
    // Seed random number generator with a varying value
    randomProvider->seed(timeProvider->millis());
}

void EngineSimulator::initialize() {
    // Zero out entire structure
    memset(&status, 0, sizeof(EngineStatus));
    
    // Set command response
    status.response = 'A';
    
    // Initialize to cold engine state
    currentMode = EngineMode::STARTUP;
    engineStartTime = timeProvider->millis();
    lastUpdateTime = engineStartTime;
    stateStartTime = engineStartTime;
    
    // Cold start conditions
    currentRPM = 0;
    targetRPM = RPM_IDLE_MIN + 200;  // High idle when cold
    coolantTemp = TEMP_AMBIENT;      // 20°C
    intakeTemp = TEMP_AMBIENT;
    exhaustTemp = TEMP_AMBIENT;
    currentThrottle = TPS_IDLE;
    targetThrottle = TPS_IDLE;
    
    // Initial sensor values
    status.setRPM(currentRPM);
    status.setCoolantTemp(coolantTemp / 10);
    status.setIntakeTemp(intakeTemp / 10);
    status.setMAP(MAP_ATMOSPHERIC);
    status.batteryv = VOLTAGE_NORMAL / 10;
    status.baro = BARO_SEALEVEL;
    status.tps = currentThrottle;
    
    loopCounter = 0;
    secondCounter = 0;
}

bool EngineSimulator::update() {
    uint32_t currentTime = timeProvider->millis();
    uint32_t deltaTime = currentTime - lastUpdateTime;
    
    // Update at configured interval (default 50ms = 20Hz)
    if (deltaTime < UPDATE_INTERVAL_MS) {
        return false;
    }
    
    lastUpdateTime = currentTime;
    loopCounter++;
    
    // Update second counter
    if (loopCounter % 20 == 0) {  // Every second at 20Hz
        secondCounter++;
        status.secl = secondCounter & 0xFF;  // Wrap at 256
    }
    
    // Update state machine
    updateStateMachine();
    
    // Simulate all engine parameters in realistic order
    simulateRPM();          // Engine speed drives everything
    simulateThermal();      // Temperature affects fuel/timing
    simulateThrottle();     // Throttle position
    simulateMAP();          // Manifold pressure from RPM & throttle
    simulateFuel();         // Fuel delivery based on MAP, RPM, temp
    simulateIgnition();     // Timing based on RPM & load
    simulateAFR();          // Air-fuel ratio and O2 sensors
    simulateCorrections();  // Fuel/timing corrections
    simulateSensors();      // Additional sensors
    simulateVoltage();      // Battery voltage
    simulateCANData();      // CAN bus data
    
    // Update counters and status
    uint16_t loops = loopCounter & 0xFFFF;
    status.loopslo = loops & 0xFF;
    status.loopshi = (loops >> 8) & 0xFF;
    
    // Simulate free RAM (more on ESP32, less on AVR)
    #ifdef ARDUINO_AVR
        status.freeramlo = 512 & 0xFF;
        status.freeramhi = (512 >> 8) & 0xFF;
    #else
        status.freeramlo = 8192 & 0xFF;
        status.freeramhi = (8192 >> 8) & 0xFF;
    #endif
    
    // Random error code (mostly no errors)
    status.errors = randomProvider->random(100) < 2 ? randomProvider->random(1, 4) : 0;
    
    return true;
}

void EngineSimulator::updateStateMachine() {
    uint32_t timeInState = timeProvider->millis() - stateStartTime;
    
    switch (currentMode) {
        case EngineMode::STARTUP:
            // Crank for 1-2 seconds, then start
            if (timeInState > 1000) {
                if (currentRPM > RPM_IDLE_MIN / 2) {
                    transitionToMode(EngineMode::WARMUP_IDLE);
                }
            }
            break;
            
        case EngineMode::WARMUP_IDLE:
            // Warm up until coolant reaches 60°C (takes ~30s)
            if (coolantTemp > 600) {  // 60°C
                transitionToMode(EngineMode::IDLE);
            }
            break;
            
        case EngineMode::IDLE:
            // Randomly transition to other modes
            if (timeInState > STATE_TRANSITION_MS) {
                int rand = randomProvider->random(100);
                if (rand < 30) {
                    transitionToMode(EngineMode::LIGHT_LOAD);
                } else if (rand < 35) {
                    transitionToMode(EngineMode::ACCELERATION);
                }
            }
            break;
            
        case EngineMode::LIGHT_LOAD:
            if (timeInState > STATE_TRANSITION_MS) {
                int rand = randomProvider->random(100);
                if (rand < 40) {
                    transitionToMode(EngineMode::ACCELERATION);
                } else if (rand < 70) {
                    transitionToMode(EngineMode::DECELERATION);
                } else {
                    transitionToMode(EngineMode::IDLE);
                }
            }
            break;
            
        case EngineMode::ACCELERATION:
            if (currentRPM > RPM_HIGH_START) {
                transitionToMode(EngineMode::HIGH_RPM);
            } else if (timeInState > 3000 && randomProvider->random(100) < 30) {
                transitionToMode(EngineMode::LIGHT_LOAD);
            }
            break;
            
        case EngineMode::HIGH_RPM:
            if (timeInState > 2000) {
                transitionToMode(EngineMode::DECELERATION);
            }
            break;
            
        case EngineMode::DECELERATION:
            if (currentRPM < RPM_IDLE_MAX + 200) {
                transitionToMode(EngineMode::IDLE);
            }
            break;
            
        case EngineMode::WOT:
            if (timeInState > 3000 || currentRPM > RPM_REDLINE) {
                transitionToMode(EngineMode::HIGH_RPM);
            }
            break;
    }
}

void EngineSimulator::transitionToMode(EngineMode newMode) {
    currentMode = newMode;
    stateStartTime = timeProvider->millis();
    
    // Set target values based on new mode
    switch (newMode) {
        case EngineMode::STARTUP:
            targetRPM = RPM_IDLE_MIN + 200;
            targetThrottle = TPS_IDLE + 5;
            rpmAcceleration = 500;  // RPM/s
            break;
            
        case EngineMode::WARMUP_IDLE:
            targetRPM = RPM_IDLE_MIN + 150;
            targetThrottle = TPS_IDLE + 3;
            rpmAcceleration = 100;
            break;
            
        case EngineMode::IDLE:
            targetRPM = RPM_IDLE_MIN + randomProvider->random(-50, 50);
            targetThrottle = TPS_IDLE;
            rpmAcceleration = 50;
            break;
            
        case EngineMode::LIGHT_LOAD:
            targetRPM = RPM_CRUISE + randomProvider->random(-300, 300);
            targetThrottle = TPS_CRUISE + randomProvider->random(-5, 10);
            rpmAcceleration = 200;
            break;
            
        case EngineMode::ACCELERATION:
            targetRPM = RPM_HIGH_START + randomProvider->random(-500, 500);
            targetThrottle = TPS_HALF + randomProvider->random(10, 40);
            rpmAcceleration = 1000;  // Fast acceleration
            break;
            
        case EngineMode::HIGH_RPM:
            targetRPM = RPM_REDLINE - randomProvider->random(100, 500);
            targetThrottle = TPS_WOT - randomProvider->random(0, 20);
            rpmAcceleration = 500;
            break;
            
        case EngineMode::DECELERATION:
            targetRPM = RPM_IDLE_MAX + randomProvider->random(0, 500);
            targetThrottle = TPS_IDLE;
            rpmAcceleration = -800;  // Fast deceleration
            break;
            
        case EngineMode::WOT:
            targetRPM = RPM_REDLINE;
            targetThrottle = TPS_WOT;
            rpmAcceleration = 1500;  // Very fast acceleration
            break;
    }
}

void EngineSimulator::simulateRPM() {
    // Smooth interpolation toward target RPM
    if (currentRPM < targetRPM) {
        int16_t delta = (rpmAcceleration * UPDATE_INTERVAL_MS) / 1000;
        currentRPM += delta;
        if (currentRPM > targetRPM) currentRPM = targetRPM;
    } else if (currentRPM > targetRPM) {
        int16_t delta = (rpmAcceleration * UPDATE_INTERVAL_MS) / 1000;
        currentRPM += delta;  // rpmAcceleration is negative
        if (currentRPM < targetRPM) currentRPM = targetRPM;
    }
    
    // Clamp to valid range
    if ((int16_t)currentRPM < RPM_MIN) currentRPM = RPM_MIN;
    if (currentRPM > RPM_MAX) currentRPM = RPM_MAX;
    
    // Add realistic idle fluctuation
    if (currentMode == EngineMode::IDLE || currentMode == EngineMode::WARMUP_IDLE) {
        currentRPM += randomProvider->random(-10, 10);
    }
    
    status.setRPM(currentRPM);
    status.setRPMDot(rpmAcceleration);
}

void EngineSimulator::simulateThermal() {
    int16_t targetCoolantTemp = TEMP_ENGINE_WARM;
    
    if (currentMode == EngineMode::WOT || currentMode == EngineMode::HIGH_RPM) {
        targetCoolantTemp = TEMP_ENGINE_HOT;
    } else if (currentMode == EngineMode::IDLE || currentMode == EngineMode::WARMUP_IDLE) {
        targetCoolantTemp = TEMP_ENGINE_WARM - 50;
    }
    
    // Gradual warmup (thermal inertia)
    coolantTemp = interpolate(coolantTemp, targetCoolantTemp, 5);  // Slow change
    status.setCoolantTemp(coolantTemp / 10);
    
    // Intake air temperature affected by engine bay heat and airflow
    int16_t targetIntakeTemp = TEMP_AMBIENT + (coolantTemp - TEMP_AMBIENT) / 4;
    if (currentRPM > RPM_CRUISE) {
        // More airflow = cooler intake
        targetIntakeTemp -= (currentRPM - RPM_CRUISE) / 50;
    }
    intakeTemp = interpolate(intakeTemp, targetIntakeTemp, 10);
    status.setIntakeTemp(intakeTemp / 10);
}

void EngineSimulator::simulateThrottle() {
    // Smooth throttle response
    currentThrottle = interpolate(currentThrottle, targetThrottle, 20);
    
    // Add realistic sensor noise
    int8_t noisyThrottle = currentThrottle + addNoise(0, 1);
    if (noisyThrottle < 0) noisyThrottle = 0;
    if (noisyThrottle > 100) noisyThrottle = 100;
    
    status.tps = noisyThrottle;
    status.tpsadc = (noisyThrottle * 255) / 100;  // Scale to ADC range
    
    // TPS rate of change
    static uint8_t lastTPS = 0;
    int16_t tpsDelta = status.tps - lastTPS;
    status.tpsdot = tpsDelta * (1000 / UPDATE_INTERVAL_MS);  // Convert to %/s
    lastTPS = status.tps;
}

void EngineSimulator::simulateMAP() {
    // Manifold pressure based on RPM and throttle
    // At idle: low MAP (35-40 kPa)
    // At WOT: near atmospheric (95-100 kPa)
    // Cruise: intermediate (50-70 kPa)
    
    uint16_t baseMAP;
    
    if (currentThrottle < 10) {
        // Idle/closed throttle: high vacuum
        baseMAP = MAP_IDLE + (currentRPM - RPM_IDLE_MIN) / 20;
    } else if (currentThrottle > 80) {
        // WOT: near atmospheric
        baseMAP = MAP_WOT - (RPM_MAX - currentRPM) / 100;
    } else {
        // Proportional to throttle
        baseMAP = mapValue(currentThrottle, 10, 80, MAP_IDLE + 10, MAP_WOT - 5);
    }
    
    // RPM affects pumping efficiency
    if (currentRPM > RPM_HIGH_START) {
        baseMAP += (currentRPM - RPM_HIGH_START) / 100;
    }
    
    // Add sensor noise
    uint16_t noisyMAP = baseMAP + addNoise(0, 2);
    if (noisyMAP > MAP_ATMOSPHERIC) noisyMAP = MAP_ATMOSPHERIC;
    
    status.setMAP(noisyMAP);
}

void EngineSimulator::simulateFuel() {
    // Calculate volumetric efficiency
    status.ve = calculateVE(currentRPM, currentThrottle);
    
    // Calculate required pulse width based on MAP, RPM, and VE
    uint16_t map = status.getMAP();
    pulseWidth = calculateRequiredPulseWidth(currentRPM, map, status.ve);
    
    // Apply warm-up enrichment
    uint8_t wue = getWarmupEnrichment(coolantTemp);
    pulseWidth = (pulseWidth * wue) / 100;
    status.wue = wue;
    
    // Apply corrections
    uint16_t correctedPW = (pulseWidth * status.egocorrection) / 100;
    correctedPW = (correctedPW * status.iatcorrection) / 100;
    
    // Clamp to valid range
    if (correctedPW < PW_MIN) correctedPW = PW_MIN;
    if (correctedPW > PW_MAX) correctedPW = PW_MAX;
    
    status.setPulseWidth(correctedPW);
    
    // Acceleration enrichment
    if (status.tpsdot > 10) {
        status.taeamount = 100 + status.tpsdot / 2;
    } else {
        status.taeamount = 100;
    }
    
    // Total fuel correction
    status.gammae = (status.egocorrection * status.iatcorrection * status.wue) / 10000;
}

void EngineSimulator::simulateIgnition() {
    // Calculate ignition advance based on RPM and load
    uint8_t load = (status.getMAP() * 100) / MAP_ATMOSPHERIC;
    status.advance = calculateIgnitionAdvance(currentRPM, load);
    
    // Dwell time (coil charge time) based on voltage and RPM
    // Typical: 3-4ms at 14V, increases at low voltage
    uint16_t baseDwell = 35;  // 3.5ms in 0.1ms units
    if (status.batteryv < 12) {
        baseDwell = 45;  // 4.5ms at low voltage
    }
    status.dwell = baseDwell;
    
    // Spark flags (example: bit 0 = spark enabled)
    status.spark = 0x01;
}

void EngineSimulator::simulateAFR() {
    // Target AFR based on engine mode
    uint8_t targetAFR;
    
    switch (currentMode) {
        case EngineMode::STARTUP:
        case EngineMode::WARMUP_IDLE:
            targetAFR = AFR_RICH;  // Rich during warmup
            break;
        case EngineMode::WOT:
        case EngineMode::ACCELERATION:
            targetAFR = AFR_WOT;   // Rich for power
            break;
        case EngineMode::DECELERATION:
            targetAFR = AFR_LEAN;  // Lean during decel (fuel cut)
            break;
        default:
            targetAFR = AFR_STOICH;  // Stoichiometric for efficiency
            break;
    }
    
    status.afrtarget = targetAFR;
    
    // Simulate O2 sensor reading
    // O2 value: 0-255 maps to lambda 0.5-1.5
    // Lambda = AFR / 14.7
    uint16_t lambda = (targetAFR * 100) / 147;  // Lambda * 100
    status.o2 = mapValue(lambda, 50, 150, 0, 255);
    status.o2_2 = status.o2 + addNoise(0, 3);  // Secondary sensor
    
    // Add realistic sensor noise and response lag
    status.o2 = status.o2 + addNoise(0, 5);
}

void EngineSimulator::simulateCorrections() {
    // EGO (O2) correction: center at 100%
    // In closed loop, oscillates around stoich
    if (coolantTemp > 500 && currentMode != EngineMode::WOT) {
        // Closed loop active
        static int8_t egoTrend = 1;
        status.egocorrection += egoTrend;
        if (status.egocorrection > 110) egoTrend = -1;
        if (status.egocorrection < 90) egoTrend = 1;
    } else {
        // Open loop
        status.egocorrection = 100;
    }
    
    // IAT correction: richer when intake air is cold
    int16_t iatCelsius = intakeTemp / 10;
    if (iatCelsius < 0) {
        status.iatcorrection = 110;  // 10% enrichment
    } else if (iatCelsius < 10) {
        status.iatcorrection = 105;  // 5% enrichment
    } else {
        status.iatcorrection = 100;  // No correction
    }
    
    // Battery voltage correction
    if (status.batteryv < 12) {
        status.batcorrection = 105;  // Increase pulse width at low voltage
    } else {
        status.batcorrection = 100;
    }
    
    // Flex fuel (not used in this simulation, set to gasoline)
    status.ethanolpct = 0;
    status.flexcorrection = 100;
    status.flexigncorrection = 0;
    
    // Idle load
    status.idleload = (currentMode == EngineMode::IDLE) ? 
                      (30 + randomProvider->random(-5, 5)) : 0;
    
    // Boost (N/A engine, no boost)
    status.boosttarget = 0;
    status.boostduty = 0;
}

void EngineSimulator::simulateSensors() {
    // Status flags (example bitfields)
    status.status1 = 0x00;
    if (currentRPM > 0) status.status1 |= 0x01;  // Engine running
    if (coolantTemp > 500) status.status1 |= 0x02;  // Warm
    
    status.engine = 0x00;
    if (currentMode == EngineMode::STARTUP) status.engine |= 0x01;  // Cranking
    if (currentRPM > 0) status.engine |= 0x02;  // Running
    
    // Test outputs
    status.testoutputs = 0x00;
}

void EngineSimulator::simulateVoltage() {
    // Battery voltage fluctuates based on load
    uint8_t baseVoltage = VOLTAGE_NORMAL / 10;
    
    if (currentMode == EngineMode::STARTUP) {
        baseVoltage = 10;  // Voltage drops during cranking
    } else if (currentRPM > RPM_CRUISE) {
        baseVoltage = 14;  // Alternator charging
    }
    
    status.batteryv = baseVoltage + addNoise(0, 1);
}

void EngineSimulator::simulateCANData() {
    // Fill CAN data array with realistic values
    // Example: RPM, coolant temp, etc. encoded for CAN bus
    
    // Bytes 0-1: RPM (big-endian for CAN)
    status.canin[0] = (currentRPM >> 8) & 0xFF;
    status.canin[1] = currentRPM & 0xFF;
    
    // Bytes 2-3: Vehicle speed (simulated from RPM and gear)
    uint16_t speed = (currentRPM / 100);  // Simplified: ~100 RPM = 1 km/h
    if (speed > 255) speed = 255;
    status.canin[2] = speed & 0xFF;
    status.canin[3] = 0;
    
    // Bytes 4-5: Coolant temp
    status.canin[4] = status.clt;
    status.canin[5] = 0;
    
    // Bytes 6-7: TPS
    status.canin[6] = status.tps;
    status.canin[7] = 0;
    
    // Fill remaining with pattern for testing
    for (int i = 8; i < 32; i++) {
        status.canin[i] = (i * 7 + loopCounter) & 0xFF;
    }
}

// Helper functions

uint8_t EngineSimulator::calculateVE(uint16_t rpm, uint8_t tps) {
    // Volumetric efficiency curve for typical I4 engine
    // Peak VE around 3500-5000 RPM
    
    uint8_t baseVE;
    
    if (rpm < 1000) {
        baseVE = 45;
    } else if (rpm < 2000) {
        baseVE = 55 + (rpm - 1000) / 50;
    } else if (rpm < 4000) {
        baseVE = 75 + (rpm - 2000) / 100;
    } else if (rpm < 5500) {
        baseVE = 85 + (rpm - 4000) / 200;
    } else {
        baseVE = 90 - (rpm - 5500) / 100;  // Falls off at high RPM
    }
    
    // Throttle affects VE
    baseVE = (baseVE * (50 + tps / 2)) / 100;
    
    if (baseVE > 100) baseVE = 100;
    if (baseVE < 30) baseVE = 30;
    
    return baseVE;
}

uint8_t EngineSimulator::calculateIgnitionAdvance(uint16_t rpm, uint8_t load) {
    // Ignition timing map: more advance at higher RPM and lower load
    
    uint8_t baseAdvance = TIMING_IDLE;
    
    if (rpm > 1000) {
        baseAdvance += (rpm - 1000) / 200;
    }
    
    // Reduce advance at high load (prevent knock)
    if (load > 80) {
        baseAdvance -= (load - 80) / 4;
    } else if (load < 40) {
        // More advance at light load
        baseAdvance += (40 - load) / 8;
    }
    
    // Clamp to safe range
    if (baseAdvance > TIMING_MAX) baseAdvance = TIMING_MAX;
    if (baseAdvance < 5) baseAdvance = 5;
    
    return baseAdvance;
}

uint16_t EngineSimulator::calculateRequiredPulseWidth(uint16_t rpm, uint16_t map, uint8_t ve) {
    // Simplified fuel calculation
    // Real formula: PW = (MAP * displacement * VE) / (RPM * AFR * injector_flow)
    
    // Base pulse width proportional to MAP and VE, inversely to RPM
    uint32_t pw = (uint32_t)map * ve * 1000;
    pw = pw / (rpm + 1);  // Avoid division by zero
    pw = pw / 10;  // Scale factor
    
    // Clamp to reasonable range
    if (pw < PW_MIN) pw = PW_MIN;
    if (pw > PW_MAX) pw = PW_MAX;
    
    return (uint16_t)pw;
}

uint8_t EngineSimulator::getWarmupEnrichment(int16_t coolantTemp) {
    // Warmer engine needs less enrichment
    int16_t tempC = coolantTemp / 10;
    
    if (tempC < 0) return 140;      // 40% enrichment when very cold
    if (tempC < 20) return 130;     // 30% enrichment when cold
    if (tempC < 40) return 120;     // 20% enrichment
    if (tempC < 60) return 110;     // 10% enrichment
    return 100;                      // No enrichment when warm
}

int8_t EngineSimulator::addNoise(int8_t value, int8_t range) {
    #if SENSOR_NOISE_ENABLED
        return value + randomProvider->random(-range, range + 1);
    #else
        return value;
    #endif
}

int16_t EngineSimulator::interpolate(int16_t current, int16_t target, uint8_t rate) {
    // Linear interpolation with rate limiting
    int16_t delta = target - current;
    int16_t maxChange = (delta * rate) / 100;
    
    if (maxChange == 0 && delta != 0) {
        maxChange = (delta > 0) ? 1 : -1;
    }
    
    return current + maxChange;
}

uint16_t EngineSimulator::mapValue(uint16_t x, uint16_t in_min, uint16_t in_max, 
                                    uint16_t out_min, uint16_t out_max) {
    // Arduino map() function equivalent
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

void EngineSimulator::setMode(EngineMode mode) {
    transitionToMode(mode);
}

uint32_t EngineSimulator::getRuntime() const {
    return (timeProvider->millis() - engineStartTime) / 1000;
}
