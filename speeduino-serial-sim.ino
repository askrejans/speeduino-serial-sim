#include "ecu/EngineStatus.h"
#include "ecu/EngineSimulation.cpp"

// Constants for serial communication and engine simulation
#define SERIAL_BAUD_RATE 115200
#define COMMAND_TRIGGER 'A'
#define SERIAL_READ_DELAY 20

// Global variable to hold real-time engine status
EngineStatus engineStatus;

/**
 * @brief Setup function executed once at program start.
 * 
 * Initializes serial communication and engine status.
 */
void setup() {
  Serial.begin(SERIAL_BAUD_RATE);
  initializeEngineStatus(engineStatus);
}

/**
 * @brief Loop function executed repeatedly.
 * 
 * Listens for commands from the serial port and responds with simulated engine data.
 * Uses the 'A' command to trigger data generation.
 */
void loop() {
  static unsigned long lastRpmChangeTime = 0;
  static bool increasingRpm = true;

  // Simulate RPM changes every RPM_CHANGE_INTERVAL milliseconds
  if (millis() - lastRpmChangeTime >= RPM_CHANGE_INTERVAL) {
    if (increasingRpm) {
      // Simulate increasing RPM
      engineStatus.rpmhi += 500;
      if (engineStatus.rpmhi >= MAX_RPM) {
        engineStatus.rpmhi = MAX_RPM;
        increasingRpm = false;
      }
    } else {
      // Simulate decreasing RPM
      engineStatus.rpmhi -= 500;
      if (engineStatus.rpmhi <= IDLE_RPM) {
        engineStatus.rpmhi = IDLE_RPM;
        increasingRpm = true;
      }
    }

    lastRpmChangeTime = millis();
  }

  // Simulate other parameters based on RPM
  simulateParameters(engineStatus);

  // Send simulated engine data
  if (Serial.available() > 0) {
    char command = Serial.read();
    if (command == COMMAND_TRIGGER) {
      generateSimulatedEngineData(engineStatus);
      Serial.write((uint8_t*)&engineStatus, sizeof(engineStatus));
    }
  }

  delay(SERIAL_READ_DELAY);
}
