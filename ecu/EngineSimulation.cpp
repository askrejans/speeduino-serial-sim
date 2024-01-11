#include "EngineStatus.h"

// Constants for simulation
#define IDLE_RPM 800
#define MAX_RPM 5000
#define RPM_CHANGE_INTERVAL 5000  // Time interval for RPM change in milliseconds

void simulateParameters(EngineStatus& engineStatus) {
  // Simulate temperature increase with RPM
  engineStatus.iat = map(engineStatus.rpmhi, IDLE_RPM, MAX_RPM, 30, 80);

  // Simulate RPM-dependent parameters
  engineStatus.rpmdotlo = map(engineStatus.rpmhi, IDLE_RPM, MAX_RPM, 0, 500);
  engineStatus.rpmdothi = map(engineStatus.rpmhi, IDLE_RPM, MAX_RPM, 0, 500);
  engineStatus.ve = map(engineStatus.rpmhi, IDLE_RPM, MAX_RPM, 80, 120);
  engineStatus.tps = map(engineStatus.rpmhi, IDLE_RPM, MAX_RPM, 0, 100);
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
  // Simulate idling warmed-up engine
  engineStatus.response = 'A';
  engineStatus.secl = (byte)(millis() / 1000);

  // Simulate random engine parameter variations
  engineStatus.rpmlo = random(0, 20);
  engineStatus.rpmhi = random(120, 140);
  engineStatus.tps = random(0, 5);
  engineStatus.idleload = random(80, 100);
  engineStatus.advance = 10;

  // Reset flags and status
  engineStatus.status1 = 0;
  engineStatus.engine = 1;
  engineStatus.spark = 0;
  engineStatus.testoutputs = 0;

  // Simulate random values for other engine parameters
  engineStatus.dwell = (byte)(random(5, 25) * 10);
  engineStatus.maplo = random(80, 120);
  engineStatus.maphi = 0;
  engineStatus.iat = random(30, 50);
  engineStatus.clt = random(70, 90);
  engineStatus.batcorrection = random(120, 140);
  engineStatus.batteryv = (byte)(random(1200, 1400) / 10.0);
  engineStatus.o2 = (byte)(random(900, 1300) / 10.0);
  engineStatus.egocorrection = random(120, 140);
  engineStatus.iatcorrection = random(120, 140);
  engineStatus.wue = random(120, 140);
  engineStatus.taeamount = random(120, 140);
  engineStatus.gammae = random(120, 140);
  engineStatus.ve = random(120, 140);
  engineStatus.afrtarget = random(120, 140);
  engineStatus.pw1lo = random(120, 140);
  engineStatus.pw1hi = random(120, 140);
  engineStatus.tpsdot = random(120, 140);
  engineStatus.advance = random(18, 22);
  engineStatus.tps = random(0, 100);
  engineStatus.loopslo = random(120, 140);
  engineStatus.loopshi = random(120, 140);
  engineStatus.freeramlo = random(120, 140);
  engineStatus.freeramhi = random(120, 140);
  engineStatus.boosttarget = random(120, 140);
  engineStatus.boostduty = random(120, 140);
  engineStatus.spark = random(0, 255);
  engineStatus.rpmdotlo = random(0, 500);
  engineStatus.rpmdothi = random(0, 500);
  engineStatus.ethanolpct = random(120, 140);
  engineStatus.flexcorrection = random(120, 140);
  engineStatus.flexigncorrection = random(120, 140);
  engineStatus.idleload = random(120, 140);
  engineStatus.testoutputs = random(0, 255);
  engineStatus.o2_2 = random(120, 140);
  engineStatus.baro = random(120, 140);

  // Simulate random values for CAN input data
  for (int i = 0; i < 16; i++) {
    engineStatus.canin[i] = random(120, 140);
  }

  engineStatus.tpsadc = random(120, 140);
  engineStatus.errors = getNextError();
}

byte getNextError() {
  return random(0, 3);
}
