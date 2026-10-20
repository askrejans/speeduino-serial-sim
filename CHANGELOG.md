# Changelog

## 3.0.0

### Contract changes
- Realtime packet units follow firmware 202501.6: TPS in 0.5 % steps, pulse
  width and dwell in µs, O2/AFR × 10, signed TPSdot/MAPdot/RPMdot and loads,
  oil and fuel pressure in PSI. Clients that decoded 2.x values by its own
  conventions will see different numbers; clients written for real firmware
  now read correct ones.
- Status bits follow `globals.h` (2.x swapped running/cranking and always set
  status2 bit 0, hard launch).
- `S` returns `Speeduino 2025.01.6`; `d` returns the CRC big-endian; legacy and
  framed `n` are gone (not in 202501). New legacy `c` (loops/s) and `m` (free RAM).
- Web API replaced: `/api/status`, `/api/live`, `/api/packet`, `/api/track`,
  `/api/events` (SSE), `/api/control`, `/api/settings`, `/api/wifi*`. The 2.x
  `/api/realtime`, `/api/statistics` and `/api/setmode` are removed.

### Added
- Lap simulation of Biķernieku trase from OpenStreetMap: driver, gearbox,
  clutch, launch control, out/hot/cool-down laps, pit idle.
- Engine and ECU model: manifold mass balance, torque, thermal model with
  thermostat and fan, VE/ignition tables, WUE/ASE/AE, fuel film, EGO closed
  loop, idle control, DFCO, limiters.
- Reply pacing to a 115200-baud UART (~14 ms for `A`, as a CORE4).
- Tune pages in RAM on ESP/native, so TunerStudio writes and CRCs agree.
- Web dashboard: Dash, Track, Engine, Control, System; light and dark.
- Desktop build (`pio run -e native`): TCP or pseudo-terminal, faster than
  real time, CLI settings.
- Environments for generic ESP32-S3 modules (`esp32s3-devkit`, `-usb`).
- Host unit tests with a real CORE4 capture.

### Fixed
- AVR stack overflow risk (up to ~1.3 KB of stack buffers on a 2 KB Uno).
- Cross-site scripting through scanned SSIDs on the Wi-Fi page.
- Log output corrupting the protocol port.
- ESP8266 builds used the ESP32-only AsyncTCP.
- Web handlers racing the main loop on ESP32.

### Changed
- Platforms and libraries updated (espressif32 8.0.0, espressif8266 4.2.1,
  atmelavr 5.3.0, esp32async AsyncTCP 3.5 / ESPAsyncTCP 2.0 /
  ESPAsyncWebServer 3.12); ArduinoJson no longer used.
- 2.x Wi-Fi station settings are picked up on first boot.

## 2.2.0
- Framed protocol v2 (CRC32); 130-byte packet layout; TunerStudio connects.

## 2.1.0
- Wi-Fi serial socket (TCP 5000); station mode.

## 2.0.0
- Rewrite: multi-platform, web interface, tests.
