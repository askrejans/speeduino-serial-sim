#ifndef ENGINE_STATUS_H
#define ENGINE_STATUS_H

#include <Arduino.h>

// Structure to hold real-time engine status
struct EngineStatus {
  char response;           // Command response indicator
  byte secl;               // Seconds counter
  byte status1;            // General status flags (bitfield)
  byte engine;             // Engine status flags (bitfield)
  byte dwell;              // Ignition dwell time
  byte maplo;              // Low byte of manifold absolute pressure
  byte maphi;              // High byte of manifold absolute pressure
  byte iat;                // Intake air temperature
  byte clt;                // Coolant temperature
  byte batcorrection;      // Battery correction value (%)
  byte batteryv;           // Battery voltage (scaled)
  byte o2;                 // Primary oxygen sensor value
  byte egocorrection;      // Exhaust gas oxygen correction (%)
  byte iatcorrection;      // Intake air temperature correction (%)
  byte wue;                // Warm-up enrichment (%)
  byte rpmlo;              // Low byte of RPM
  byte rpmhi;              // High byte of RPM
  byte taeamount;          // Throttle angle enrichment amount (%)
  byte gammae;             // Exhaust gas recirculation correction (%)
  byte ve;                 // Volumetric efficiency (%)
  byte afrtarget;          // Air-fuel ratio target
  byte pw1lo;              // Low byte of injector pulse width
  byte pw1hi;              // High byte of injector pulse width
  byte tpsdot;             // Throttle position sensor rate of change
  byte advance;            // Ignition advance
  byte tps;                // Throttle position sensor value
  byte loopslo;            // Low byte of control loop counters
  byte loopshi;            // High byte of control loop counters
  byte freeramlo;          // Low byte of free RAM
  byte freeramhi;          // High byte of free RAM
  byte boosttarget;        // Boost pressure target
  byte boostduty;          // Boost duty cycle
  byte spark;              // Spark-related flags (bitfield)
  byte rpmdotlo;           // Low byte of RPM rate of change
  byte rpmdothi;           // High byte of RPM rate of change
  byte ethanolpct;         // Ethanol percentage
  byte flexcorrection;     // Flex fuel correction (% above or below 100)
  byte flexigncorrection;  // Flex fuel ignition correction (Increased degrees of advance)
  byte idleload;           // Idle load
  byte testoutputs;        // Test output flags (bitfield)
  byte o2_2;               // Secondary oxygen sensor value
  byte baro;               // Barometric pressure
  byte canin[32];          // Controller Area Network input data
  byte tpsadc;             // Throttle position sensor analog-to-digital converter value
  byte errors;             // Error code
};

// Function prototypes
void initializeEngineStatus(EngineStatus& engineStatus);
void generateSimulatedEngineData(EngineStatus& engineStatus);
byte getNextError();

#endif // ENGINE_STATUS_H
