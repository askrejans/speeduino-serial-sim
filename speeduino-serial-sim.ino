#include "ecu/EngineStatus.h"        // Include the header file containing the EngineStatus struct
#include "ecu/EngineSimulation.cpp"  // Include the implementation file for engine simulation

#define SERIAL_BAUD_RATE 115200

EngineStatus engineStatus;  // Global variable to hold real-time engine status

/**
 * @brief Setup function executed once at program start
 * 
 * Initialize serial communication and engine status.
 */
void setup() {
  Serial.begin(SERIAL_BAUD_RATE);        // Initialize serial communication
  initializeEngineStatus(engineStatus);  // Initialize the engine status
}

/**
 * @brief Loop function executed repeatedly
 * 
 * Listen for commands from the serial port and respond with simulated engine data.
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
    if (command == 'A') {
      Serial.write((uint8_t*)&engineStatus, sizeof(engineStatus));
    }
  }

  delay(20);
}
