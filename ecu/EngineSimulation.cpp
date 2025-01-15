#include "EngineStatus.h"
#include <Arduino.h>

// Constants for simulation
#define IDLE_RPM 800
#define MAX_RPM 7000
#define RPM_CHANGE_INTERVAL 5000  // Time interval for RPM change in milliseconds

enum EngineMode {
  STARTUP,
  IDLING,
  ACCELERATION,
  HIGH_RPM,
  DECELERATION
};

void simulateParameters(EngineStatus& engineStatus) {
  // Simulate temperature increase with RPM using a quadratic curve
  engineStatus.iat = 30 + 0.00001 * pow(engineStatus.rpmhi, 2);

  // Simulate RPM-dependent parameters using more advanced curves
  engineStatus.rpmdotlo = 0.1 * engineStatus.rpmhi;
  engineStatus.rpmdothi = 0.1 * engineStatus.rpmhi;
  engineStatus.ve = 80 + 0.00002 * pow(engineStatus.rpmhi, 2);
  engineStatus.tps = 0.01 * engineStatus.rpmhi;
  engineStatus.batteryv = 12 + 0.0003 * engineStatus.rpmhi;
  engineStatus.maplo = 80 + 0.00002 * pow(engineStatus.rpmhi, 2);
  engineStatus.maphi = 80 + 0.00002 * pow(engineStatus.rpmhi, 2);
  engineStatus.spark = 0.036 * engineStatus.rpmhi;
  engineStatus.advance = 10 + 0.002 * engineStatus.rpmhi;
  engineStatus.dwell = 10 + 0.002 * engineStatus.rpmhi;
}

void initializeEngineStatus(EngineStatus& engineStatus) {
  // Initialize engine status with default values
  engineStatus.response = 'A';
  engineStatus.secl = 0;
  engineStatus.status1 = 0;
  engineStatus.engine = 0;
  engineStatus.dwell = 0;
  engineStatus.maplo = 0;
  engineStatus.maphi = 0;
  engineStatus.iat = 0;
  engineStatus.clt = 0;
  engineStatus.batcorrection = 0;
  engineStatus.batteryv = 0;
  engineStatus.o2 = 0;
  engineStatus.egocorrection = 0;
  engineStatus.iatcorrection = 0;
  engineStatus.wue = 0;
  engineStatus.rpmlo = 0;
  engineStatus.rpmhi = 0;
  engineStatus.taeamount = 0;
  engineStatus.gammae = 0;
  engineStatus.ve = 0;
  engineStatus.afrtarget = 0;
  engineStatus.pw1lo = 0;
  engineStatus.pw1hi = 0;
  engineStatus.tpsdot = 0;
  engineStatus.advance = 0;
  engineStatus.tps = 0;
  engineStatus.loopslo = 0;
  engineStatus.loopshi = 0;
  engineStatus.freeramlo = 0;
  engineStatus.freeramhi = 0;
  engineStatus.boosttarget = 0;
  engineStatus.boostduty = 0;
  engineStatus.spark = 0;
  engineStatus.rpmdotlo = 0;
  engineStatus.rpmdothi = 0;
  engineStatus.ethanolpct = 0;
  engineStatus.flexcorrection = 0;
  engineStatus.flexigncorrection = 0;
  engineStatus.idleload = 0;
  engineStatus.testoutputs = 0;
  engineStatus.o2_2 = 0;
  engineStatus.baro = 0;

  for (int i = 0; i < 32; i++) {
    engineStatus.canin[i] = 0;
  }

  engineStatus.tpsadc = 0;
  engineStatus.errors = 0;
}

void generateSimulatedEngineData(EngineStatus& engineStatus) {
  static unsigned long lastUpdateTime = 0;
  static EngineMode mode = STARTUP;
  unsigned long currentTime = millis();
  unsigned long elapsedTime = currentTime - lastUpdateTime;

  if (elapsedTime >= RPM_CHANGE_INTERVAL) {
    lastUpdateTime = currentTime;

    // Simulate engine startup, idling, acceleration, high RPM, and deceleration
    switch (mode) {
      case STARTUP:
        engineStatus.rpmhi = random(800, 1200);
        engineStatus.clt = random(30, 50); // Cold start temperature
        mode = IDLING;
        break;
      case IDLING:
        engineStatus.rpmhi = random(800, 1000);
        engineStatus.clt = random(50, 70); // Warming up
        if (engineStatus.clt > 60) {
          mode = ACCELERATION;
        }
        break;
      case ACCELERATION:
        engineStatus.rpmhi = random(1000, 3000);
        engineStatus.clt = random(70, 90); // Normal operating temperature
        if (engineStatus.rpmhi > 2500) {
          mode = HIGH_RPM;
        }
        break;
      case HIGH_RPM:
        engineStatus.rpmhi = random(3000, 7000);
        engineStatus.clt = random(80, 100); // High load temperature
        if (engineStatus.rpmhi < 3500) {
          mode = DECELERATION;
        }
        break;
      case DECELERATION:
        engineStatus.rpmhi = random(1000, 3000);
        engineStatus.clt = random(70, 90); // Cooling down
        if (engineStatus.rpmhi < 1500) {
          mode = IDLING;
        }
        break;
    }

    // Simulate other engine parameters based on RPM and mode
    engineStatus.tps = 0.01 * engineStatus.rpmhi;
    engineStatus.idleload = 80 + 0.00002 * pow(engineStatus.rpmhi, 2);
    engineStatus.advance = 10 + 0.002 * engineStatus.rpmhi;
    engineStatus.dwell = 10 + 0.002 * engineStatus.rpmhi;
    engineStatus.maplo = 80 + 0.00002 * pow(engineStatus.rpmhi, 2);
    engineStatus.maphi = 80 + 0.00002 * pow(engineStatus.rpmhi, 2);
    engineStatus.batteryv = 12 + 0.0003 * engineStatus.rpmhi;
    engineStatus.o2 = 0.9 + 0.00005 * engineStatus.rpmhi;
    engineStatus.ve = 80 + 0.00002 * pow(engineStatus.rpmhi, 2);
    engineStatus.afrtarget = 14 - 0.0003 * engineStatus.rpmhi;
    engineStatus.pw1lo = 1 + 0.0005 * engineStatus.rpmhi;
    engineStatus.pw1hi = 5 + 0.0005 * engineStatus.rpmhi;
    engineStatus.tpsdot = 0.01 * engineStatus.rpmhi;
    engineStatus.boosttarget = 0.002 * engineStatus.rpmhi;
    engineStatus.boostduty = 0.01 * engineStatus.rpmhi;
    engineStatus.spark = 0.036 * engineStatus.rpmhi;
    engineStatus.rpmdotlo = 0.1 * engineStatus.rpmhi;
    engineStatus.rpmdothi = 0.1 * engineStatus.rpmhi;
    engineStatus.ethanolpct = 0.01 * engineStatus.rpmhi;
    engineStatus.flexcorrection = 0.01 * engineStatus.rpmhi;
    engineStatus.flexigncorrection = 0.01 * engineStatus.rpmhi;
    engineStatus.idleload = 80 + 0.00002 * pow(engineStatus.rpmhi, 2);
    engineStatus.testoutputs = 0.036 * engineStatus.rpmhi;
    engineStatus.o2_2 = 0.9 + 0.00005 * engineStatus.rpmhi;
    engineStatus.baro = 100 + 0.0001 * engineStatus.rpmhi;

    // Simulate random values for CAN input data
    for (int i = 0; i < 16; i++) {
      engineStatus.canin[i] = random(120, 140);
    }

    engineStatus.tpsadc = random(120, 140);
    engineStatus.errors = getNextError();
  }
}

byte getNextError() {
  return random(0, 3);
}