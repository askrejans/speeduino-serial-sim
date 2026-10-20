# speeduino-serial-sim

A Speeduino ECU you can plug in without an engine. Firmware for ESP32,
ESP8266 and Arduino boards (plus a desktop build) that speaks the Speeduino
202501 serial protocol byte for byte. Behind it, a car laps Biķernieku trase
(Rīga) with a physical driver, gearbox, engine and Speeduino's own fuel and
ignition logic, so every channel moves the way it does on a real car.

Use it to develop and test dashboards, loggers, gateways (such as
[ecu-to-mqtt](https://github.com/askrejans/ecu-to-mqtt)) and TunerStudio
setups on the bench.

## What it does

- **Identifies as a real controller.** `Q` → `speeduino 202501`, `S` →
  `Speeduino 2025.01.6`, `F` → `002`: the strings a DIY-EFI CORE4 on firmware
  202501.6 returns. Legacy commands (`A`, `r`, `Q`, `S`, `C`, `F`, `c`, `m`,
  `d`) and the framed TunerStudio protocol (`r` 0x30, `p`, `M`, `d`, `b`, `f`,
  `C` …) follow `comms.cpp` / `comms_legacy.cpp` of that release.
- **Sends the real 130-byte packet in the real units.** Layout from
  `getTSLogEntry()`, units from the firmware: TPS in 0.5 % steps, pulse width
  and dwell in µs, signed rate fields, AFR × 10, pressures in PSI. Compared in
  the tests against a capture from a real CORE4.
- **Answers at real speed.** Replies are paced to a 115200-baud UART (~14 ms
  for `A`), even over USB CDC or Wi-Fi.
- **Laps Biķernieki.** A 1.8 L 16-valve hatchback on semi-slicks, driven to a
  speed envelope built from the OpenStreetMap outline: launch control at the
  pit exit, out-lap, hot laps (~2:02), cool-down lap, pit idle, again. Shifts
  at 6800 rpm, downshifts with throttle blips, DFCO on lift-off, rev limiters.
- **Engine and ECU, not curves.** Intake manifold by mass balance (WOT
  ~99 kPa, overrun ~15 kPa, idle ~35 kPa), torque from MAP, coolant with
  thermostat, radiator airflow and fan, intake heat soak, oil temperature and
  pressure. ECU side: VE and ignition tables, warm-up/after-start/acceleration
  enrichment, a fuel film on tip-ins, EGO closed loop, closed-loop idle,
  launch control and spark-cut limiters, every status bit.
- **Web dashboard** (ESP): Dash, Track (live map, laps, speed trace), Engine
  (the decoded packet and raw bytes), Control (engine and session, pace,
  temperatures) and System (protocol counters, Wi-Fi), light and dark themes.
- **Simplified on small boards.** On an Uno or Nano the same driver and
  engine run on a 10 m track table at 50 Hz without noise, EGO and the
  detailed thermal model, in 2 KB of RAM.

## Boards

| Environment | Board | Protocol on | Extras |
|---|---|---|---|
| `esp32s3` (default) | Seeed XIAO ESP32-S3 | TCP 5000 over Wi-Fi (logs on USB) | web dashboard |
| `esp32s3-devkit` | ESP32-S3 DevKitC and generic S3 modules | TCP 5000 over Wi-Fi (logs on USB) | web dashboard |
| `esp32s3-devkit-usb` | same | USB, like a real board | web dashboard |
| `esp32`, `esp32-wrover`, `esp32s2` | ESP32 DevKit, WROVER, S2 | UART / USB serial | web dashboard |
| `esp8266`, `d1_mini` | NodeMCU, Wemos D1 mini | UART | web dashboard |
| `uno`, `nano`, `mega` | ATmega328P / 2560 | UART | compact model |
| `native` | macOS / Linux | TCP port or pseudo-terminal | desktop simulator, unit tests |

## Quick start

```bash
pio run -e esp32s3-devkit-usb -t upload     # or your board's environment
```

Then point any Speeduino client at the board's serial port at 115200 baud,
for example:

```bash
ECU_TO_MQTT_MQTT_ENABLED=false ECU_TO_MQTT_HTTP_ENABLED=true ecu-to-mqtt
# finds the board by its signature; dashboard at http://127.0.0.1:9091
```

No board at hand? Run it on the desktop:

```bash
pio run -e native
.pio/build/native/program                 # Speeduino on tcp://127.0.0.1:5000
.pio/build/native/program --pty           # or on a /dev/ttys… pseudo-terminal
.pio/build/native/program --speed 10      # ten times real time
```

Options: `--bind`, `--port`, `--pty`, `--speed`, `--pace 0.5…1.05`,
`--laps N`, `--ambient C`, `--coolant C`, `--no-launch`, `--no-noise`,
`--quiet`.

## Wi-Fi and the web dashboard

ESP boards start an access point **SpeeduinoSim** (password `speeduino123`)
with the dashboard at <http://192.168.4.1>. To put the board on your network,
open **System**, pick the network, enter its password and press **Join and
restart**. If the network is unreachable at boot the access point comes back.
mDNS: `http://speeduino-sim.local`.

| Mode (key) | Content |
|---|---|
| Dash (1) | ECU status lamps (running, sync, warm-up, ASE, accel enrich, DFCO, idle control, limiters, launch, fan) and gauges |
| Track (2) | Biķernieki outline with the car, the current and previous lap coloured by speed, lap table with deltas, speed trace against the best lap |
| Engine (3) | The 130-byte packet decoded with firmware units, raw bytes, every status bit |
| Control (4) | Start or stop the engine, go out, box this lap, blip; pace, hot laps, ambient and key-on coolant, launch control, noise, automatic sessions |
| System (5) | Protocol counters (commands, realtime reads, CRC errors, last command), device, Wi-Fi |

The dashboard has no external resources and a strict Content-Security-Policy.
The HTTP API is in [docs/API.md](docs/API.md). The control endpoints are not
authenticated: run the board on a bench network you trust.

## TunerStudio

Use the Speeduino 202501 INI. Connect over USB (`esp32s3-devkit-usb`, AVR
boards, ESP32 on UART) or over TCP (`<board IP>:5000` on the Wi-Fi builds).
The handshake, signature and live gauges work. Tune pages read as zeros
until written. ESP boards keep TunerStudio's page writes in RAM, so CRC checks
agree until the next reset; AVR boards acknowledge writes without storing
them. It is not a tuning target: tables do not change how the simulated
engine runs.

## Accuracy

The packet layout, units, identity strings and command replies follow the
firmware source of release 202501.6 and were checked against a real
DIY-EFI CORE4. Speeds, temperatures, pressures and timings come from a
physical model calibrated to a typical 1.8 L 16V; they are realistic, not a
recording of a particular car. See [TECHNICAL.md](TECHNICAL.md).

## Development

```bash
pio test -e native                          # protocol, packet and model tests on the host
pio run                                     # default board
pio run -e uno -e esp32 -e esp8266          # other targets
python3 scripts/gen_track.py data/tracks/bikernieki.json include/TrackData.h
```

`web/` is embedded at build time by `scripts/embed_web.py`. AI-assisted
development guidance is in [AGENTS.md](AGENTS.md) and [skills/](skills/).

## Related projects

- [ecu-to-mqtt](https://github.com/askrejans/ecu-to-mqtt): ECU to MQTT/HTTP gateway (reads this simulator like a real Speeduino)
- [gps-to-mqtt](https://github.com/askrejans/gps-to-mqtt): companion GNSS gateway
- [io-to-net](https://github.com/askrejans/io-to-net): serial-to-TCP bridge

## Licence

MIT. See [LICENSE](LICENSE). Track data © OpenStreetMap contributors, ODbL-1.0
([data/tracks/bikernieki.json](data/tracks/bikernieki.json)).
