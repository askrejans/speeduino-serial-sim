# Speeduino Serial Simulator - v2.0.0

A cross-platform Speeduino ECU serial interface simulator with realistic I4 engine simulation, supporting Arduino AVR, ESP32, and ESP8266. Tested more on ESP32 for now.

## 🚀 Features

### Core Features
- **Realistic Engine Simulation**: Physics-based I4 2.0L engine model with correlated parameters
- **Speeduino Protocol Compatible**: Implements 5 most common commands (A, Q, V, S, n)
- **Cross-Platform**: Arduino AVR, ESP32, ESP8266

### Platform-Specific Features

#### Arduino AVR (Minimal Mode)
- Basic engine simulation
- Serial protocol support
- Optimized for 2KB RAM / 32KB flash
- Status LED feedback

#### ESP32/ESP8266 (Full Features)
- Advanced realistic engine physics
- WiFi connectivity
- Web-based dashboard
- Real-time parameter monitoring
- Remote control via REST API
- mDNS support (speeduino-sim.local)

## 📋 Requirements

### Hardware
- **Arduino**: Uno, Mega, Nano (ATmega328P/2560)
- **ESP32**: DevKit, WROOM, WROVER, S2, S3
- **ESP8266**: NodeMCU, D1 Mini

### Software
- PlatformIO Core or PlatformIO IDE

## 🔧 Installation

### Quick Start with PlatformIO

```bash
# Clone repository
git clone https://github.com/askrejans/speeduino-serial-sim.git
cd speeduino-serial-sim

# Build for Arduino Uno
pio run -e uno

# Build for ESP32
pio run -e esp32

# Upload to device
pio run -e uno -t upload

# Monitor serial output
pio device monitor
```

### Arduino IDE (Legacy Support)

The project is now PlatformIO-based. For Arduino IDE users:
1. Copy all files from `include/` and `src/` into a single sketch folder
2. Rename `main.cpp` to match your sketch name
3. Note: Web interface and advanced features require PlatformIO

## 📡 Usage

### Serial Communication

Connect to device via serial port at **115200 baud**:

```bash
# Linux/Mac
screen /dev/ttyUSB0 115200

# Windows - Use PuTTY or Arduino Serial Monitor
```

#### Supported Commands

| Command | Description | Response Size |
|---------|-------------|---------------|
| `A` | Get real-time engine data | 79 bytes |
| `Q` | ECU status | 4 bytes |
| `V` | Firmware version | Variable |
| `S` | ECU signature | 20 bytes |
| `n` | Configuration page sizes | 7 bytes |

### Web Interface (ESP32/ESP8266 Only)

1. **Power on device** - Creates WiFi AP "SpeeduinoSim"
2. **Connect** with password: `speeduino123`
3. **Open browser** to `http://192.168.4.1`
4. **Monitor** real-time engine parameters
5. **Control** simulation mode (Idle, Acceleration, WOT, etc.)

#### REST API Endpoints

```bash
# Get real-time data (JSON)
curl http://192.168.4.1/api/realtime

# Get status
curl http://192.168.4.1/api/status

# Set engine mode
curl -X POST http://192.168.4.1/api/setmode -d "mode=wot"
```

Available modes: `idle`, `light_load`, `acceleration`, `high_rpm`, `wot`

## 🧪 Testing

```bash
# Run all tests
pio test -e uno
pio test -e esp32

# Specific test suites
pio test -e uno --filter test_engine_simulator
pio test -e uno --filter test_protocol
```

## 📊 Engine Simulation

### Physical Model

Simulates a **2.0L Inline-4 engine** with:
- **RPM Range**: 0-7000 RPM (redline at 6800)
- **Thermal Model**: Realistic coolant/intake temps
- **Volumetric Efficiency**: Peak at 4000-5000 RPM
- **Fuel Delivery**: Calculated from MAP, RPM, VE, temps
- **Ignition Timing**: 15-35° BTDC based on RPM/load
- **AFR Control**: Stoich (14.7:1) cruise, rich (12.5:1) WOT

### Operating Modes

STARTUP → WARMUP → IDLE ⇄ LIGHT_LOAD ⇄ ACCELERATION → HIGH_RPM → DECELERATION → IDLE

## 🔧 Configuration

Edit `include/Config.h`:

```cpp
#define SERIAL_BAUD_RATE 115200
#define ENGINE_DISPLACEMENT 2000  // cc
#define RPM_MAX 7000
#define WIFI_SSID "SpeeduinoSim"
#define WIFI_PASSWORD "speeduino123"
```

## 📈 Performance

| Platform | Flash | RAM | Update Rate | Response Time |
|----------|-------|-----|-------------|---------------|
| Arduino Uno | ~18 KB | ~1.2 KB | 20 Hz | <5 ms |
| ESP32 | ~45 KB | ~15 KB | 20 Hz | <2 ms |
| ESP8266 | ~40 KB | ~12 KB | 20 Hz | <3 ms |

## 🐛 Troubleshooting

**Serial Issues**: Check baud rate (115200), verify port  
**WiFi Issues**: Connect to AP "SpeeduinoSim", use 192.168.4.1  
**Compilation**: Use PlatformIO for best results

## 📝 Architecture

```
main.cpp → EngineSimulator (Physics) → SpeeduinoProtocol (Serial)
                ↓                              ↓
        WebInterface (ESP)            Platform Adapters
```

Key components:
- **EngineSimulator**: Realistic I4 physics
- **SpeeduinoProtocol**: Serial command handler  
- **WebInterface**: HTTP server (ESP only)
- **Platform Adapters**: Hardware abstraction

## Use Cases

- Testing [speeduino-to-mqtt](https://github.com/askrejans/speeduino-to-mqtt)
- Development with [golf86-info](https://github.com/askrejans/golf86-info) LED displays
- TunerStudio integration testing
- ECU logging software development
- Educational demonstrations

## 📜 License

MIT License - See [LICENSE](LICENSE)

## 📧 Contact

- **Author**: Arvis Krējāns
- **GitHub**: [@askrejans](https://github.com/askrejans)

---

**Version**: 2.0.0
**Major Changes from v1.0**: Complete rewrite with realistic physics, multi-platform support, web interface, comprehensive tests
