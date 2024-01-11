#include "ecu/EngineStatus.h"  // Include the header file containing the EngineStatus struct
#include "ecu/EngineSimulation.cpp"  // Include the implementation file for engine simulation

#define SERIAL_BAUD_RATE 115200

EngineStatus engineStatus;  // Global variable to hold real-time engine status

/**
 * @brief Setup function executed once at program start
 * 
 * Initialize serial communication and engine status.
 */
void setup() {
  Serial.begin(SERIAL_BAUD_RATE);  // Initialize serial communication
  initializeEngineStatus(engineStatus);  // Initialize the engine status
}

/**
 * @brief Loop function executed repeatedly
 * 
 * Listen for commands from the serial port and respond with simulated engine data.
 * Uses the 'A' command to trigger data generation.
 */
void loop() {
  if (Serial.available() > 0) {
    char command = Serial.read();
    if (command == 'A') {
      generateSimulatedEngineData(engineStatus);  // Generate simulated engine data
      Serial.write((uint8_t*)&engineStatus, sizeof(engineStatus));  // Send the data over serial
    }
  }
  delay(20);  // Add a delay to control the rate of data generation
}
