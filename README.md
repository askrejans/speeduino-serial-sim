# Speeduino Serial Simulator - v2.1.0

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
- WiFi Serial Socket (ESP32-S3 default): TCP/IP serial communication on port 5000

### WiFi Serial Socket (NEW in v2.1.0)

The feature enables serial communication over TCP/IP instead of hardware UART. This allows remote applications to connect over the network.

**Enabling WiFi Serial:**
- Add `-D ENABLE_WIFI_SERIAL` to your build flags in [platformio.ini](platformio.ini#L119)
- Enabled by default on ESP32-S3
- Available on all ESP32 and ESP8266 builds

**Usage:**
1. Build and flash with WiFi serial enabled
2. Connect to WiFi AP "SpeeduinoSim" (password: `speeduino123`)
3. Note the device IP address (shown in serial monitor or web dashboard)
4. Configure your serial application to connect to `<IP>:5000` via TCP/IP
   - **TunerStudio**: Use TCP/IP connection type instead of COM port
   - **Custom tools**: Connect TCP socket to port 5000

**How it works:**
- Hardware UART (Serial) is used for debug logging/monitoring
- TCP socket on port 5000 handles Speeduino protocol commands
- Web interface remains on port 80 (HTTP)
- Simultaneous web dashboard + remote serial access

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

**Default Access (First Boot):**
1. **Power on device** - Creates WiFi AP "SpeeduinoSim"
2. **Connect** with password: `speeduino123`
3. **Open browser** to `http://192.168.4.1` (port 80)
4. **Monitor** real-time engine parameters
5. **Control** simulation mode (Idle, Acceleration, WOT, etc.)

**WiFi Configuration:**
The device supports two WiFi modes with automatic fallback:
- **Access Point (AP) Mode**: Default mode, creates its own network at 192.168.4.1
- **Station (STA) Mode**: Connects to your existing WiFi network

To configure WiFi:
1. Access the web interface at `http://192.168.4.1` in AP mode
2. Click "WiFi Settings" button
3. Scan for available networks or manually enter SSID
4. Enter password and enable "Station Mode"
5. Restart the device
6. If connection fails, device automatically falls back to AP mode
7. Find new IP address from serial monitor or connect to AP and check settings

**Ports:**
- **80**: HTTP web dashboard
- **5000**: Speeduino serial protocol (if ENABLE_WIFI_SERIAL is defined)

#### REST API Endpoints

```bash
# Get real-time data (JSON)
curl http://192.168.4.1/api/realtime

# Get status
curl http://192.168.4.1/api/status

# Set engine mode
curl -X POST http://192.168.4.1/api/setmode -d "mode=wot"

# Available modes: idle, light_load, acceleration, high_rpm, wot

# WiFi configuration endpoints
curl http://192.168.4.1/api/wifi/scan
curl -X POST http://192.168.4.1/api/wifi/save -d "ssid=MyNetwork&password=mypass&enable=true"
curl -X POST http://192.168.4.1/api/wifi/reset
curl -X POST http://192.168.4.1/api/restart
```

### Testing WiFi Serial (TCP Socket on Port 5000)

When `ENABLE_WIFI_SERIAL` is defined, you can test Speeduino protocol commands over TCP:

```bash
# Using netcat (nc)
nc 192.168.4.1 5000
# Then type commands: A, V, Q, S, n

# Using telnet
telnet 192.168.4.1 5000
# Then type commands

# Test with echo and nc (send 'A' command)
echo -n "A" | nc 192.168.4.1 5000 | xxd

# Test version command
echo -n "V" | nc 192.168.4.1 5000

# Continuous real-time parsed data (send 'A' every 0.1 second)
while true; do   ts=$(date '+%Y-%m-%d %H:%M:%S.%3N');    exec 3<>/dev/tcp/192.168.4.1/5000;   printf "A" >&3;    frame=$(dd bs=79 count=1 <&3 2>/dev/null | xxd -p -c 79);   exec 3>&-;    if [ ${#frame} -eq 158 ]; then     rpm=$(( 0x${frame:32:2}${frame:30:2} ));     echo "$ts RPM=$rpm";   else     echo "$ts BAD_FRAME";   fi;    sleep 0.1; done
```

## 🧪 Testing

### Unit Tests

Run comprehensive embedded tests (40+ test cases):

```bash
# Run all tests on ESP32-S3 (includes WiFi/TCP tests)
platformio test -e esp32s3 --upload-port /dev/cu.usbmodem11101 --test-port /dev/cu.usbmodem11101

# Run on ESP32-S2 (WiFi tests, no serial socket)
platformio test -e esp32s2

# Run on Arduino Uno (basic tests only)
platformio test -e uno

# Verbose output for debugging
platformio test -e esp32s3 -vvv
```

See [test/README.md](test/README.md) for detailed test documentation.

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

- **GitHub**: [@askrejans](https://github.com/askrejans)

---

**Version**: 2.1.0
**Major Changes**:
- **v2.1.0**: WiFi serial socket support (TCP/IP alternative to hardware UART), WiFi station mode added
- **v2.0.0**: Complete rewrite with realistic physics, multi-platform support, web interface, comprehensive tests
