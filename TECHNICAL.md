# speeduino-serial-sim — technical review and design (3.0.0)

This document records the review of 2.2.0, what was wrong, and how 3.0.0
works. Protocol details: [docs/PROTOCOL.md](docs/PROTOCOL.md). HTTP API:
[docs/API.md](docs/API.md).

## 1. Summary

3.0.0 replaces a random state machine with a physical simulation of a car
lapping Biķernieki and Speeduino's control logic on top, and makes the serial
side indistinguishable from a real 202501.6 controller for a client:
identity strings, packet units, command replies and reply timing.

Reference hardware: a DIY-EFI CORE4 on Speeduino 202501.6. Its `Q`/`S`
replies, an engine-off packet (now a test fixture) and its 13.8 ms
request-reply time over USB were captured and compared.

## 2. Review of 2.2.0

Severity: **H** = wrong data, crash or exposure; **M** = degraded operation;
**L** = quality.

### 2.1 Correctness of the emulated controller

| # | Sev | Finding | Resolution |
|---|-----|---------|------------|
| C1 | H | Units disagreed with the firmware. TPS was sent as 0–100 % (202201+ sends 0.5 % steps), O2 as a lambda mapped to 0–255 (the firmware sends AFR × 10), pulse width and dwell in tenths of a millisecond (the firmware sends µs). Gateways decoding real firmware saw half the throttle, nonsense AFR and pulse widths 100× too small. | Setters in `EngineStatus` take physical units and encode firmware units; tests pin the encodings. |
| C2 | H | Status bits were invented: engine bit 0 was used for cranking and bit 1 for running (the reverse of `globals.h`); status2 was always `0x01`, which is *hard launch*; status1 bit 0 meant "running". | Every bit from `globals.h`; set by the logic that sets it on the ECU. |
| C3 | M | 2 % of packets carried a random error code; CAN inputs held a counting pattern. | Errors 0, CAN inputs 0, as the bench controller. |
| C4 | M | `S` returned `Speeduino 202501` (the real reply is `Speeduino 2025.01.6`); `d` byte-swapped the CRC; `n` (not in 202501) answered; `S` did not zero `secl`. | Replies as `comms.cpp`; `S` zeroes `secl` and clears status5 bit 5. |
| C5 | M | Replies were instantaneous over USB CDC and TCP; a gateway or logger tuned against the simulator saw 2 ms where a real board takes ~14 ms. | Replies paced to a 115200-baud UART. |
| C6 | M | Values were driven by a random mode machine (idle, light load, WOT …) with independent curves: MAP, RPM, throttle and fuel did not follow from each other, and nothing resembled driving. | Physical model (§3). |

### 2.2 Stability and security

| # | Sev | Finding | Resolution |
|---|-----|---------|------------|
| S1 | H | Stack use on AVR: a 512-byte payload buffer, a variable-length reply array of up to 512 bytes and a 288-byte page buffer, on a 2 KB Uno. | Replies are streamed with a running CRC; the request buffer is 16 bytes on AVR (TunerStudio's largest request is 7). |
| S2 | H | The Wi-Fi page inserted scanned SSIDs into the page with `innerHTML`: anyone in radio range could run script in the dashboard by naming a network. | The dashboard builds DOM with `textContent` only; strict CSP; JSON strings are escaped. |
| S3 | M | Boot banners and status lines were printed on the serial port the protocol uses, corrupting the first replies a client reads. | Nothing is printed on the protocol port; logs only go to USB when the protocol runs over Wi-Fi. |
| S4 | M | ESP8266 environments listed the ESP32-only `AsyncTCP`; libraries came from a personal fork's tags; `espressif32` was pinned to 6.9.0. | Official `esp32async` libraries (`ESPAsyncTCP` on ESP8266); platforms and libraries at their current releases; ArduinoJson removed (no longer needed). |
| S5 | M | Web handlers (ESP32: network task) read the simulator while the main loop wrote it. | Snapshots are published into locked buffers by the main loop; commands are queued and applied there. |
| S6 | L | Tests needed hardware attached. | Host tests (`pio test -e native`) cover protocol, packet and model. |

## 3. Model

```text
 driver ─ throttle/brake/gear ─► engine ◄─ manifold (mass balance) ◄─ throttle plate + idle valve
   ▲                               │ torque
   │ speed envelope                ▼
 track (Biķernieki, 5 m)       clutch/gearbox ─► wheels ─► car (drag, rolling, traction, brakes)
                                   │
              coolant / intake / oil temperatures, battery
                                   │
                       ECU: tables, corrections, closed loops, cuts ─► 130-byte packet
```

**Track.** `scripts/gen_track.py` takes the reviewed OpenStreetMap outline
(the one the G86 app's demo sessions use), rounds it with Chaikin corner
cutting, resamples it every 5 m (10 m on AVR), computes the grip-limited
corner speed (1.05 g) and a backward braking pass (0.95 g). The table holds
the speed envelope and heading per node; the ESP and native builds also keep
the outline for maps. Acceleration is not in the table: it comes from the
engine, gear and drag.

**Driver.** Looks 0.4 s ahead, wants to be at the envelope (scaled by pace),
asks for the force that gets there in 0.6 s, and turns it into full throttle,
a part throttle (inverting the torque and manifold model to a plate angle) or
braking. Shifts up at 6800 rpm, down while braking when the lower gear stays
below 6000 rpm, blipping the throttle. Out-laps run at 82 %, the cool-down lap
at 60 % and stops at the line.

**Car.** 1080 kg, CdA 0.64 m², 185/60 R14, 3.45/2.12/1.44/1.13/0.91 with a
3.67 final drive, 0.62 g traction limit. Clutch slip at launch, free engine
during shifts.

**Engine.** WOT torque curve peaking at 168 N·m / 4500 rpm; torque scales
with manifold pressure above 18 kPa, minus friction. The manifold settles to
the pressure where flow through the throttle (choked below 0.528 of baro)
equals the flow into the cylinders, with a filling lag that shrinks with rpm.
Calibrated results: idle 35 kPa, part throttle 66 kPa at 10 % / 3000 rpm,
WOT 99 kPa, overrun 15 kPa.

**Thermal.** Coolant heated by brake power plus a base load, cooled through a
thermostat (82–92 °C) and a radiator whose airflow follows speed and the fan
(on 96 °C, off 91 °C); 35 kJ/K. Intake air heat-soaks when slow; oil follows
coolant with load. AVR uses a simple warm-up curve instead.

**ECU.** Speed-density fuel: `PW = reqFuel × VE × MAP × gammaE + injector
dead time × battery correction`, with a VE table that is a few per cent off
the engine's true VE so the EGO loop has work to do. gammaE multiplies
warm-up, after-start (30 s), IAT density, baro, EGO and acceleration
enrichment. A fuel film on the port walls leans tip-ins and richens lift-offs
over ~0.25 s; the wideband sees it after a transport delay and a 80 ms
response. EGO: ±1 % every 100 ms in closed loop (warm, 1000–5500 rpm, under
70 % throttle), 100 % otherwise. Closed-loop idle on the idle valve, idle
ignition trim, DFCO after 0.3 s closed throttle above 1500 rpm, soft limiter
(retard) at 7000, spark-cut hard limiter at 7200, launch control at 4200.

**Measured on the host** (`pio test -e native` and the desktop build):

| | Value |
|---|---|
| Hot lap | 2:01.8–2:02.1 at pace 1.0; out-lap ~2:10 |
| Top speed | ~149 km/h, 4th/5th gear |
| Idle | 850 rpm hot (1200 cold), 34–35 kPa, PW ~2.3 ms |
| WOT | 98–100 kPa, PW 6–7 ms, AFR 12.0–13.4 |
| Cruise | AFR 14.6–15.0 with EGO trim |
| DFCO | PW 0, wideband ~24 |
| Coolant on hot laps | 88–96 °C, fan cycling in the pit |

## 4. Firmware structure

| Path | |
|---|---|
| `include/EngineStatus.h` | packed packet, unit-correct setters, bit names |
| `src/EngineSimulator.cpp` | driver, car, engine, thermal, ECU |
| `src/Track.cpp`, `include/TrackData.h` | track lookups; generated tables (PROGMEM) |
| `src/SpeeduinoProtocol.cpp` | legacy and framed protocol, tune pages, pacing |
| `src/Snapshot.cpp` | JSON for the web side |
| `src/WebInterface.cpp`, `web/` | Wi-Fi, embedded dashboard, HTTP API (ESP) |
| `src/main.cpp` | Arduino entry |
| `src/native/` | desktop build: TCP server, pseudo-terminal, CLI |
| `test/test_native/` | host tests, real-ECU fixture |

The physics runs in fixed steps (10 ms; 20 ms on AVR) from `ITimeProvider`,
catching up at most 250 ms after a stall. The protocol handles one command
per loop; replies are streamed.

## 5. Verification

* `pio test -e native`: 17 tests — packet encodings, CRC32 reference vector,
  legacy and framed replies (identity, `A`, `r`, `C`, `f`, bad CRC, unknown,
  `S` and `secl`), tune page write/read/CRC agreement, reply pacing, the
  engine-off packet against the CORE4 capture, cold start to idle, a full
  session (hot lap time, rpm range, gears, DFCO, launch, WOT, PW, coolant,
  lateral g), track closure, JSON documents and escaping.
* ESP32-S3 (N8R2 module) with the protocol on USB, read by ecu-to-mqtt 0.7.1:
  auto-detected by signature, 50 Hz, 14.0 ms per request-reply (CORE4:
  13.8 ms), every channel and status bit decoded as from the real controller.
* Same board in station mode, protocol on TCP 5000: 37 Hz at 21 ms per
  request-reply (UART pacing plus the Wi-Fi round trip, as a real Wi-Fi
  serial bridge). Writing replies byte by byte had cost 252 ms (one TCP
  segment per byte); the adapter now sends each reply in one write.
* Builds: see the README board table (all environments build).

## 6. Known limitations

* Tune pages are storage only; the simulated engine does not use TunerStudio's tables.
* One car and one track. The generator takes any closed outline, but other
  cars need new constants in `include/Vehicle.h`.
* Weather is constant per session (ambient temperature and baro are settings).
* No knock, sync loss, sensor failures or engine protection events are simulated.
* The web API is unauthenticated; it is meant for bench networks.
