#ifndef ENGINE_STATUS_H
#define ENGINE_STATUS_H

#include <Arduino.h>

// Structure to hold real-time engine status
struct EngineStatus {
  char response;           // Command response indicator
  uint8_t secl;            // Seconds counter
  uint8_t status1;         // General status flags (bitfield)
  uint8_t engine;          // Engine status flags (bitfield)
  uint8_t dwell;           // Ignition dwell time
  uint8_t maplo;           // Low byte of manifold absolute pressure
  uint8_t maphi;           // High byte of manifold absolute pressure
  uint8_t iat;             // Intake air temperature
  uint8_t clt;             // Coolant temperature
  uint8_t batcorrection;   // Battery correction value (%)
  uint8_t batteryv;        // Battery voltage (scaled)
  uint8_t o2;              // Primary oxygen sensor value
  uint8_t egocorrection;   // Exhaust gas oxygen correction (%)
  uint8_t iatcorrection;   // Intake air temperature correction (%)
  uint8_t wue;             // Warm-up enrichment (%)
  uint8_t rpmlo;           // Low byte of RPM
  uint8_t rpmhi;           // High byte of RPM
  uint8_t taeamount;       // Throttle angle enrichment amount (%)
  uint8_t gammae;          // Exhaust gas recirculation correction (%)
  uint8_t ve;              // Volumetric efficiency (%)
  uint8_t afrtarget;       // Air-fuel ratio target
  uint8_t pw1lo;           // Low byte of injector pulse width
  uint8_t pw1hi;           // High byte of injector pulse width
  uint8_t tpsdot;          // Throttle position sensor rate of change
  uint8_t advance;         // Ignition advance
  uint8_t tps;             // Throttle position sensor value
  uint8_t loopslo;         // Low byte of control loop counters
  uint8_t loopshi;         // High byte of control loop counters
  uint8_t freeramlo;       // Low byte of free RAM
  uint8_t freeramhi;       // High byte of free RAM
  uint8_t boosttarget;     // Boost pressure target
  uint8_t boostduty;       // Boost duty cycle
  uint8_t spark;           // Spark-related flags (bitfield)
  uint8_t rpmdotlo;        // Low byte of RPM rate of change
  uint8_t rpmdothi;        // High byte of RPM rate of change
  uint8_t ethanolpct;      // Ethanol percentage
  uint8_t flexcorrection;  // Flex fuel correction (% above or below 100)
  uint8_t flexigncorrection;// Flex fuel ignition correction (Increased degrees of advance)
  uint8_t idleload;        // Idle load
  uint8_t testoutputs;     // Test output flags (bitfield)
  uint8_t o2_2;            // Secondary oxygen sensor value
  uint8_t baro;            // Barometric pressure
  uint8_t canin[32];       // Controller Area Network input data
  uint8_t tpsadc;          // Throttle position sensor analog-to-digital converter value
  uint8_t errors;          // Error code
};

// Function prototypes
void initializeEngineStatus(EngineStatus& engineStatus);
void generateSimulatedEngineData(EngineStatus& engineStatus);
uint8_t getNextError();

#endif // ENGINE_STATUS_H