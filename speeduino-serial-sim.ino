// Description: Arduino code for generating and transmitting simulated real-time engine status data simulating Speeduino serial communication commands.
// Currently only A command for real-time data is supported. Simplified engine simulation included.

// Define the baud rate for serial communication
#define SERIAL_BAUD_RATE 115200

// Data structure to hold real-time engine status
struct RealTimeStatus {
  char response;           // Command response indicator
  byte secl;               // Seconds counter
  byte status1;            // General status flags
  byte engine;             // Engine status flags
  byte dwell;              // Ignition dwell time
  byte maplo;              // Low byte of manifold absolute pressure
  byte maphi;              // High byte of manifold absolute pressure
  byte iat;                // Intake air temperature
  byte clt;                // Coolant temperature
  byte batcorrection;      // Battery correction value
  byte batteryv;           // Battery voltage
  byte o2;                 // Primary oxygen sensor value
  byte egocorrection;      // Exhaust gas oxygen correction
  byte iatcorrection;      // Intake air temperature correction
  byte wue;                // Warm-up enrichment
  byte rpmlo;              // Low byte of RPM
  byte rpmhi;              // High byte of RPM
  byte taeamount;          // Throttle angle enrichment amount
  byte gammae;             // Exhaust gas recirculation correction
  byte ve;                 // Volumetric efficiency
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
  byte spark;              // Spark-related flags
  byte rpmdotlo;           // Low byte of RPM rate of change
  byte rpmdothi;           // High byte of RPM rate of change
  byte ethanolpct;         // Ethanol percentage
  byte flexcorrection;     // Flex fuel correction
  byte flexigncorrection;  // Flex fuel ignition correction
  byte idleload;           // Idle load
  byte testoutputs;        // Test output flags
  byte o2_2;               // Secondary oxygen sensor value
  byte baro;               // Barometric pressure
  byte canin[32];          // Controller Area Network input data
  byte tpsadc;             // Throttle position sensor analog-to-digital converter value
  byte errors;             // Error code
};

// Global variable to hold real-time engine status
RealTimeStatus rtStatus;

// Function prototypes
void setup();
void loop();
void generateSimulatedEngineData();
byte getNextError();

// Setup function - executed once at program start
void setup() {
  Serial.begin(SERIAL_BAUD_RATE);  // Initialize serial communication
}

// Loop function - executed repeatedly
void loop() {
  if (Serial.available() > 0) {
    char command = Serial.read();
    if (command == 'A') {
      // Respond with dummy data
      generateSimulatedEngineData();
      Serial.write((uint8_t *)&rtStatus, sizeof(rtStatus));
    }
  }
  // Add a delay to control the rate of data generation
  delay(20);
}

//roughly simulates a real running engine parameters
void generateSimulatedEngineData() {
  static byte engineState = 0;

  // Simulate idling warmed-up engine
  rtStatus.response = 'A';                  // 'A' command received confirmation
  rtStatus.secl = (byte)(millis() / 1000);  // Incrementing counter each second

  rtStatus.rpmlo = random(0, 20);  // Simulate slight RPM variations
  rtStatus.rpmhi = random(120, 140);
  rtStatus.tps = random(0, 5);          // Simulate slight throttle variations during idle
  rtStatus.idleload = random(80, 100);  // Simulate idle load variations
  rtStatus.advance = 10;                // Simulate constant ignition advance at idle


  // Update real-time data values
  rtStatus.status1 = 0;      // Clear status flags
  rtStatus.engine = 1;       // Set engine running flag
  rtStatus.spark = 0;        // Clear spark-related flags
  rtStatus.testoutputs = 0;  // Clear test output flags

  // Update other real-time data values as needed
  rtStatus.dwell = (byte)(random(5, 25) * 10);  // Dwell in ms * 10
  rtStatus.maplo = random(80, 120);
  rtStatus.maphi = 0;
  rtStatus.iat = random(30, 50);
  rtStatus.clt = random(70, 90);
  rtStatus.batcorrection = random(120, 140);
  rtStatus.batteryv = (byte)(random(1200, 1400) / 10.0);  // Battery voltage
  rtStatus.o2 = (byte)(random(900, 1300) / 10.0);         // Primary O2
  rtStatus.egocorrection = random(120, 140);
  rtStatus.iatcorrection = random(120, 140);
  rtStatus.wue = random(120, 140);
  rtStatus.taeamount = random(120, 140);
  rtStatus.gammae = random(120, 140);
  rtStatus.ve = random(120, 140);
  rtStatus.afrtarget = random(120, 140);
  rtStatus.pw1lo = random(120, 140);
  rtStatus.pw1hi = random(120, 140);
  rtStatus.tpsdot = random(120, 140);
  rtStatus.advance = random(18, 22);
  rtStatus.tps = random(0, 100);
  rtStatus.loopslo = random(120, 140);
  rtStatus.loopshi = random(120, 140);
  rtStatus.freeramlo = random(120, 140);
  rtStatus.freeramhi = random(120, 140);
  rtStatus.boosttarget = random(120, 140);
  rtStatus.boostduty = random(120, 140);
  rtStatus.spark = random(0, 255);  // Simulate spark-related flags
  rtStatus.rpmdotlo = random(0, 500);
  rtStatus.rpmdothi = random(0, 500);
  rtStatus.ethanolpct = random(120, 140);
  rtStatus.flexcorrection = random(120, 140);
  rtStatus.flexigncorrection = random(120, 140);
  rtStatus.idleload = random(120, 140);
  rtStatus.testoutputs = random(0, 255);  // Simulate test output flags
  rtStatus.o2_2 = random(120, 140);
  rtStatus.baro = random(120, 140);

  // Sample canin data
  for (int i = 0; i < 16; i++) {
    rtStatus.canin[i] = random(120, 140);
  }

  rtStatus.tpsadc = random(120, 140);
  rtStatus.errors = getNextError();
}

// Function to get the next simulated error code
byte getNextError() {
  // Simulate error codes (modify as needed)
  return random(0, 3);  // Return a random error code (0, 1, or 2)
}
