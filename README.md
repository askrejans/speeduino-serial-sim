# Speeduino Serial Simulator

This Arduino sketch provides a basic simulation of a Speeduino ECU serial interface, generating and transmitting simulated real-time engine status data. The simulation includes a simplified engine model and supports the 'A' command for real-time data. This solution could be useful when quickly testing logging, gauge or other Speeduino ECU implementations without the ECU itself. Hopefully more detailed serial interface simulation will be added in future releases.

## Table of Contents
- [Prerequisites](#prerequisites)
- [Installation](#installation)
- [Usage](#usage)
- [Customization](#customization)
- [License](#license)

## Prerequisites
Make sure you have the following tools installed before running the simulator:
- [Arduino IDE](https://www.arduino.cc/en/Main/Software)

## Installation
1. Clone the repository
2. Open the `speeduino-serial-sim.ino` file using the Arduino IDE.
3. Connect your Arduino board to your computer.
4. Select the appropriate board and port in the Arduino IDE.
5. Upload the sketch to your Arduino board.

## Usage
1. Upload the sketch to your Arduino board.
2. Open the Arduino Serial Monitor (or any other serial communication tool) to view the simulated engine data.
3. The sketch listens for the 'A' command, and upon receiving it, responds with simulated real-time engine data.

## Customization
Feel free to customize the simulation parameters and add more features according to your needs. The key parts of the code for customization are:

- The `EngineStatus` struct: Modify this structure to include or exclude specific engine parameters.
- The `EngineSimulation.cpp` class: Adjust the simulation logic to better match your requirements.
- The baud rate: You can change the baud rate by modifying the `SERIAL_BAUD_RATE` constant.

## Compatibility
This simulator is a basic implementation and just barely been tested for compatibility with a real Speeduino ECU system. It currently supports only the 'A' command for basic real-time data.

## Use cases
Can be used together with [speeduino-to-mqtt](https://github.com/askrejans/speeduino-to-mqtt) project to generate test data and [golf86-info](https://github.com/askrejans/golf86-info) to display in on LED matrix display. This was the original use case that this was created for: to test pushing parameters to MQTT and display without physical ECU.

## License
This project is licensed under the [MIT License](LICENSE). Feel free to modify and distribute it as needed.

**Note:** This simulator is a basic implementation and may not fully represent the complexity of a real Speeduino ECU system. Use it for educational or testing purposes only.
