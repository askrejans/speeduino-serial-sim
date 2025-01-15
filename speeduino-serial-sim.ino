#include "ecu/EngineStatus.h"
#include "ecu/EngineSimulation.cpp"

// Constants for serial communication
#define SERIAL_BAUD_RATE 115200
#define COMMAND_TRIGGER 'A'

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
  // Generate simulated engine data
  generateSimulatedEngineData(engineStatus);

  // Send simulated engine data
  if (Serial.available() > 0) {
    char command = Serial.read();
    if (command == COMMAND_TRIGGER) {
      Serial.write((uint8_t*)&engineStatus, sizeof(engineStatus));
    }
  }
}