# Speeduino Serial Simulator

Speeduino ECU serial protocol simulator with a realistic engine model and full Speeduino protocol implementation. Streams live telemetry to tools like TunerStudio via UART or WiFi TCP serial. Runs on Arduino, ESP32, and ESP8266 for testing dashboards, loggers, and ECU integrations without real hardware.

<img width="1536" height="1024" alt="ChatGPT Image Mar 4, 2026, 10_27_27 AM" src="https://github.com/user-attachments/assets/37d17935-6fb9-4c8f-ba68-a8355210c294" />

## Features

### Core Features
- **Realistic Engine Simulation**: Physics-based I4 2.0L engine model with correlated parameters
- **Full Speeduino Protocol v2**: Implements both legacy (single-byte) and framed (CRC32) protocol used by TunerStudio
- **Accurate Output Channels**: 130-byte `EngineStatus` struct byte layout matches Speeduino 202501 `getTSLogEntry()` exactly
- **TunerStudio Compatible**: Connects, streams live gauge data at ~20 Hz via TCP or UART
- **Cross-Platform**: Arduino AVR, ESP32, ESP8266

<img width="1233" height="790" alt="Screenshot 2026-03-10 at 00 22 27" src="https://github.com/user-attachments/assets/e28e141e-77c1-4093-82c8-48897f7d8c04" />


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

## Requirements

### Hardware
- **Arduino**: Uno, Mega, Nano (ATmega328P/2560)
- **ESP32**: DevKit, WROOM, WROVER, S2, S3
- **ESP8266**: NodeMCU, D1 Mini

### Software
- PlatformIO Core or PlatformIO IDE

## Installation

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

## Speeduino Protocol Implementation

The simulator implements the full **Speeduino serial protocol v2** (framed CRC32) as used by TunerStudio and compatible tools, alongside the legacy single-byte protocol used for initial handshake and other utilities.

### Protocol Layers

| Layer | Format | Used For |
|-------|--------|----------|
| **Legacy** | `[1B cmd]` → raw bytes, no framing | Initial port-check (`F`, `Q`, `S`), simple tools |
| **Framed v2** | `[2B BE length][payload][4B BE CRC32]` | All TunerStudio communication after handshake |

### TunerStudio Connection Sequence

TunerStudio performs the following handshake before streaming live data:

1. **Legacy `F`** → raw `"002"` — signals protocol version 2; TS switches to framed mode
2. **Framed `C`** → `{0x00, 0xFF}` — comms test OK
3. **Framed `Q`** → `{0x00, "speeduino 202501"}` — firmware version (must match `signature=` in the INI)
4. **Framed `S`** → `{0x00, "Speeduino 202501"}` — product identity string
5. **Framed `r` (sub-cmd `0x30`)** → `{0x00, outputChannels[offset..offset+len]}` — live data, repeated at ~20 Hz

### Implemented Commands

| Command | Mode | Description |
|---------|------|-------------|
| `F` | Legacy + Framed | Protocol version → `"002"` |
| `Q` | Legacy + Framed | Firmware version string (`speeduino 202501`) |
| `S` | Legacy + Framed | Product identity string |
| `C` | Legacy + Framed | Comms test |
| `V` / `v` | Legacy | Version string |
| `A` | Legacy + Framed | Realtime data dump (full 130-byte struct) |
| `r` | Legacy + Framed | Optimised output-channel read (offset + length) |
| `p` | Framed | Read tune page (returns zeros — no real tune) |
| `d` | Framed | Page CRC32 (computed from zero page) |
| `n` | Legacy + Framed | Page count and sizes |
| `b` / `B` | Framed | Burn to EEPROM (acknowledged, nothing written) |
| `I` | Framed | CAN ID (`0x00`) |
| `E` | Framed | Command button ACK |
| `f` | Framed | Serial capability struct |
| `X`,`O`,`J`,`H` | Legacy | Logger start commands → `0x01` ACK |
| `x`,`o`,`j`,`h` | Legacy | Logger stop commands → silent |

### Output Channels (130-byte struct)

The `EngineStatus` struct is packed to **exactly 130 bytes** with field order matching Speeduino 202501 `getTSLogEntry()` byte-for-byte. Key fields:

| Bytes | Field | Notes |
|-------|-------|-------|
| 0 | `secl` | Seconds counter (wraps at 256) |
| 4–5 | `mapLo/Hi` | MAP kPa (little-endian) |
| 6 | `iat` | Intake temp (°C + 40) |
| 7 | `clt` | Coolant temp (°C + 40) |
| 9 | `battery10` | Battery voltage × 10 |
| 14–15 | `rpmLo/Hi` | RPM (little-endian) |
| 19–20 | `ve1`, `ve2` | VE table outputs (%) |
| 21 | `afrTarget` | Target AFR × 10 |
| 24 | `advance` | Ignition advance (°BTDC) |
| 25 | `tps` | Throttle position (%) |
| 76–77 | `pw1Lo/Hi` | Injector PW1 (µs/100) |
| 90–91 | `dwellLo/Hi` | Dwell time |
| 102 | `ve` | Current VE (%) |

### TunerStudio Behaviour

When connecting TunerStudio with a Speeduino 202501 INI file:

- **Connection succeeds** — handshake completes, signature matches, live data streams
- **Gauges display realistic values** — RPM, MAP, CLT, IAT, TPS, AFR, advance, VE, battery, pulse width, dwell all update in real time
- **Data is correlated** — values change together logically (e.g. MAP rises with RPM, corrections respond to temperature)
- **Tune pages return zeros** — this is not a real ECU; VE tables, ignition maps, and all configuration pages are empty
- **EEPROM burn/restore errors** — TunerStudio may report CRC mismatches or warn that the tune looks unconfigured; this is expected
- **Some dashboard items may show fault states** — features that depend on valid tune data (fuel trim targets, sensor calibrations) will appear as zero or out-of-range

Despite the above warnings, all realtime gauge data is readable and realistic, making the simulator useful for:
- Testing dashboard and logging software without a running engine
- Validating Speeduino-compatible serial parsers
- Demonstrating ECU data streams in educational settings

### Compatibility with Other Software

Any software that speaks the Speeduino serial protocol should work:

| Tool | Notes |
|------|-------|
| **TunerStudio** | Connects via TCP (WiFi serial) or UART; gauges work, tune editing shows empty pages |
| **SpeedyLoader** | Legacy `A` command; 130-byte raw struct |
| **speeduino-to-mqtt** | Reads output channels over TCP/serial |
| **Custom tools / `nc`** | Legacy single-byte commands work directly |
| **Any Speeduino logger** | Framed `r` sub-command `0x30` at any offset/length |

---

## Usage

### Serial Communication

Connect to device via serial port at **115200 baud**:

```bash
# Linux/Mac
screen /dev/ttyUSB0 115200

# Windows - Use PuTTY or Arduino Serial Monitor
```

#### Supported Commands

See the **Protocol Implementation** section above for the full command reference. Quick summary:

| Command | Description |
|---------|-------------|
| `A` | Realtime data (130-byte struct) |
| `r` | Optimised output-channel read (offset + length) |
| `Q` | Firmware version string |
| `S` | Product identity string |
| `F` | Protocol version (`002`) |
| `C` | Comms test |
| `n` | Page count and sizes |
| `p` | Read tune page (zeros) |
| `b`/`B` | Burn to EEPROM (ACK only) |

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

## Testing

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

## Engine Simulation

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

## Configuration

Edit `include/Config.h`:

```cpp
#define SERIAL_BAUD_RATE 115200
#define ENGINE_DISPLACEMENT 2000  // cc
#define RPM_MAX 7000
#define WIFI_SSID "SpeeduinoSim"
#define WIFI_PASSWORD "speeduino123"
```

## Performance

| Platform | Flash | RAM | Update Rate | Response Time |
|----------|-------|-----|-------------|---------------|
| Arduino Uno | ~18 KB | ~1.2 KB | 20 Hz | <5 ms |
| ESP32 | ~45 KB | ~15 KB | 20 Hz | <2 ms |
| ESP8266 | ~40 KB | ~12 KB | 20 Hz | <3 ms |

## Troubleshooting

**Serial Issues**: Check baud rate (115200), verify port  
**WiFi Issues**: Connect to AP "SpeeduinoSim", use 192.168.4.1  
**Compilation**: Use PlatformIO for best results

## Architecture

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

## License

MIT License - See [LICENSE](LICENSE)

## Contact

- **GitHub**: [@askrejans](https://github.com/askrejans)

---

**Version**: 2.2.0
**Major Changes**:
- **v2.2.0**: Full Speeduino framed protocol v2 (CRC32) implemented; `EngineStatus` rewritten to 130-byte layout matching Speeduino 202501 `getTSLogEntry()` exactly; TunerStudio connects and reads realistic gauge data; WebInterface field names updated
- **v2.1.0**: WiFi serial socket support (TCP/IP alternative to hardware UART), WiFi station mode added
- **v2.0.0**: Complete rewrite with realistic physics, multi-platform support, web interface, comprehensive tests
