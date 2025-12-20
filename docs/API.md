# API Documentation

## EngineSimulator Class

Realistic I4 engine physics simulator with state machine.

### Constructor

```cpp
EngineSimulator(ITimeProvider* timeProvider, IRandomProvider* randomProvider)
```

**Parameters**:
- `timeProvider`: Time abstraction (millis/micros)
- `randomProvider`: Random number generator

**Example**:
```cpp
ITimeProvider* time = createTimeProvider();
IRandomProvider* random = createRandomProvider();
EngineSimulator* sim = new EngineSimulator(time, random);
```

---

### Methods

#### initialize()

```cpp
void initialize()
```

Initialize engine to cold start state. Resets all parameters.

**Must be called** before first `update()`.

---

#### update()

```cpp
bool update()
```

Update simulation step (call at 20Hz / 50ms intervals).

**Returns**: `true` if state changed, `false` otherwise

**Example**:
```cpp
void loop() {
    if (sim->update()) {
        Serial.println("State changed");
    }
    delay(50);
}
```

---

#### getStatus()

```cpp
const EngineStatus& getStatus() const
```

Get current engine status structure.

**Returns**: Reference to 79-byte `EngineStatus`

**Example**:
```cpp
const EngineStatus& status = sim->getStatus();
uint16_t rpm = status.getRPM();
int8_t temp = status.getCoolantTemp();
```

---

#### getMode()

```cpp
EngineMode getMode() const
```

Get current operating mode.

**Returns**: One of:
- `EngineMode::STARTUP`
- `EngineMode::WARMUP_IDLE`
- `EngineMode::IDLE`
- `EngineMode::LIGHT_LOAD`
- `EngineMode::ACCELERATION`
- `EngineMode::HIGH_RPM`
- `EngineMode::DECELERATION`
- `EngineMode::WOT`

---

#### setMode()

```cpp
void setMode(EngineMode mode)
```

Force mode change (for testing/control).

**Parameters**:
- `mode`: Desired engine mode

**Example**:
```cpp
sim->setMode(EngineMode::WOT);  // Force wide-open throttle
```

---

#### getRuntime()

```cpp
uint32_t getRuntime() const
```

Get elapsed time since `initialize()`.

**Returns**: Seconds since engine start

---

## SpeeduinoProtocol Class

Serial protocol handler for Speeduino commands.

### Constructor

```cpp
SpeeduinoProtocol(ISerialInterface* serial, EngineSimulator* simulator)
```

**Parameters**:
- `serial`: Serial communication interface
- `simulator`: Engine simulator data source

---

### Methods

#### begin()

```cpp
void begin()
```

Initialize protocol handler and serial port.

---

#### processCommands()

```cpp
bool processCommands()
```

Process incoming serial commands (call in loop).

**Returns**: `true` if command was processed

**Example**:
```cpp
void loop() {
    if (protocol->processCommands()) {
        // Command received and processed
    }
}
```

---

#### getCommandCount()

```cpp
uint32_t getCommandCount() const
```

Get total commands processed since start.

**Returns**: Command counter

---

#### getErrorCount()

```cpp
uint32_t getErrorCount() const
```

Get count of invalid commands received.

**Returns**: Error counter

---

## WebInterface Class

*(ESP32/ESP8266 only)*

Web-based monitoring and control interface.

### Constructor

```cpp
WebInterface(EngineSimulator* simulator, SpeeduinoProtocol* protocol)
```

---

### Methods

#### begin()

```cpp
bool begin(bool apMode = true)
```

Initialize WiFi and web server.

**Parameters**:
- `apMode`: `true` for AP mode, `false` for station mode

**Returns**: `true` if successful

**Example**:
```cpp
WebInterface* web = new WebInterface(sim, protocol);
if (web->begin(true)) {
    Serial.println("Web interface at: " + web->getIP().toString());
}
```

---

#### isConnected()

```cpp
bool isConnected() const
```

Check WiFi connection status.

**Returns**: `true` if connected

---

#### getIP()

```cpp
IPAddress getIP() const
```

Get current IP address.

**Returns**: IP address object

---

#### update()

```cpp
void update()
```

Update WiFi and handle reconnection (call in loop).

---

### REST API Endpoints

#### GET /api/status

Returns JSON with simulator status.

**Response**:
```json
{
  "mode": "idle",
  "runtime": 123,
  "connected": true,
  "ip": "192.168.4.1"
}
```

---

#### GET /api/realtime

Returns JSON with real-time engine data.

**Response**:
```json
{
  "rpm": 850,
  "clt": 85,
  "iat": 30,
  "map": 40,
  "tps": 5,
  "afr": 14.7,
  "advance": 15,
  "pw": 2.5,
  "battery": 14.0,
  "ve": 75
}
```

---

#### GET /api/statistics

Returns JSON with statistics.

**Response**:
```json
{
  "mode": "Idle",
  "runtime": 123,
  "commands": 5432,
  "errors": 3
}
```

---

#### POST /api/setmode

Change engine operating mode.

**Parameters**:
- `mode`: One of `idle`, `light_load`, `acceleration`, `high_rpm`, `wot`

**Example**:
```bash
curl -X POST http://192.168.4.1/api/setmode -d "mode=wot"
```

**Response**:
```json
{"success": true}
```

---

## EngineStatus Structure

79-byte packed structure for real-time data.

### Helper Methods

#### setRPM() / getRPM()

```cpp
void setRPM(uint16_t rpm)
uint16_t getRPM() const
```

Set/get engine RPM (handles little-endian conversion).

---

#### setMAP() / getMAP()

```cpp
void setMAP(uint16_t map)
uint16_t getMAP() const
```

Set/get manifold absolute pressure in kPa.

---

#### setPulseWidth() / getPulseWidth()

```cpp
void setPulseWidth(uint16_t pw)
uint16_t getPulseWidth() const
```

Set/get injector pulse width (0.1ms units).

---

#### setCoolantTemp() / getCoolantTemp()

```cpp
void setCoolantTemp(int8_t celsius)
int8_t getCoolantTemp() const
```

Set/get coolant temperature in °C (handles +40 offset).

---

#### setIntakeTemp() / getIntakeTemp()

```cpp
void setIntakeTemp(int8_t celsius)
int8_t getIntakeTemp() const
```

Set/get intake air temperature in °C.

---

## Platform Adapters

### Factory Functions

```cpp
ISerialInterface* createSerialInterface()
ITimeProvider* createTimeProvider()
IRandomProvider* createRandomProvider()
```

Create platform-appropriate implementations.

**Example**:
```cpp
ISerialInterface* serial = createSerialInterface();
serial->begin(115200);
serial->write((uint8_t*)"Hello", 5);
```

---

## Configuration Constants

Defined in `Config.h`:

### Serial
- `SERIAL_BAUD_RATE`: 115200
- `SERIAL_TIMEOUT_MS`: 100

### Engine
- `RPM_MIN` / `RPM_MAX`: 0 / 7000
- `RPM_IDLE_MIN` / `RPM_IDLE_MAX`: 700 / 900
- `TEMP_ENGINE_WARM`: 800 (80°C × 10)
- `MAP_ATMOSPHERIC`: 100 kPa

### WiFi (ESP only)
- `WIFI_SSID`: "SpeeduinoSim"
- `WIFI_PASSWORD`: "speeduino123"
- `WEB_SERVER_PORT`: 80

---

## Example: Complete Application

```cpp
#include "EngineSimulator.h"
#include "SpeeduinoProtocol.h"
#include "PlatformAdapters.h"

ISerialInterface* serial;
ITimeProvider* time;
IRandomProvider* random;
EngineSimulator* sim;
SpeeduinoProtocol* protocol;

void setup() {
    serial = createSerialInterface();
    time = createTimeProvider();
    random = createRandomProvider();
    
    sim = new EngineSimulator(time, random);
    sim->initialize();
    
    protocol = new SpeeduinoProtocol(serial, sim);
    protocol->begin();
}

void loop() {
    sim->update();
    protocol->processCommands();
    delay(50);
}
```
