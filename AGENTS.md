# speeduino-serial-sim — agent guide

Firmware (PlatformIO, Arduino framework) that emulates a Speeduino 202501.6
on a serial line or TCP, driven by a lap simulation of Biķernieki. Part of
`g86racing`: read the [project guidance](../AGENTS.md) before changing anything
a client relies on (packet bytes, units, identity, timing). Preserve the
current branch and any local edits.

Design and review: [TECHNICAL.md](TECHNICAL.md). Protocol:
[docs/PROTOCOL.md](docs/PROTOCOL.md). HTTP API: [docs/API.md](docs/API.md).

## Skills

| Task | Skill |
|------|-------|
| Packet fields, units, status bits, command replies, pacing | [skills/protocol-fidelity](skills/protocol-fidelity/SKILL.md) |
| Driver, car, engine, thermal or ECU behaviour; the track | [skills/lap-model](skills/lap-model/SKILL.md) |
| Web dashboard (`web/`) and HTTP API | [skills/web-dashboard](skills/web-dashboard/SKILL.md) |
| Flashing and testing on boards, comparing with a real ECU | [skills/hardware-validation](skills/hardware-validation/SKILL.md) |
| Version, platforms/libraries, release binaries | [skills/release](skills/release/SKILL.md) |
| Cross-component telemetry contracts | [../skills/automotive-telemetry](../skills/automotive-telemetry/SKILL.md) |
| Lap/sector consumers of the simulated data | [../skills/race-timing-analysis](../skills/race-timing-analysis/SKILL.md) |

## Map

```
include/EngineStatus.h   the 130-byte packet, unit-correct setters, bit names
src/EngineSimulator.cpp  driver, car, engine, thermal, ECU logic
src/Track.cpp            track lookups      include/TrackData.h  generated (gen_track.py)
include/Vehicle.h        car constants and calibration
src/SpeeduinoProtocol.cpp legacy + framed protocol, tune pages, pacing
src/Snapshot.cpp         JSON documents     src/WebInterface.cpp  Wi-Fi, API (ESP)
web/                     dashboard, embedded by scripts/embed_web.py
src/main.cpp             Arduino entry      src/native/          desktop build
test/test_native/        host tests, CORE4 fixture
```

## Commands

```bash
pio test -e native                         # must pass
pio run -e native && .pio/build/native/program --speed 10    # watch a session
pio run -e uno -e esp8266 -e esp32s3       # small, mid and large targets must build
python3 scripts/gen_track.py data/tracks/bikernieki.json include/TrackData.h
```

## Invariants

1. **Firmware is the reference.** Packet layout, units, identity strings and
   replies follow release 202501.6 source (`comms.cpp`, `comms_legacy.cpp`,
   `logger.cpp`, `globals.h`, `sensors.cpp`, its INI). Wiki pages are older.
   Changes need the source line that justifies them.
2. **Set packet fields through the `EngineStatus` setters** (physical units in,
   firmware units out). Never write a raw scaled byte from the model.
3. **Nothing else on the protocol port.** No banners, no logs.
4. **AVR fits.** 2 KB RAM on an Uno: no buffers over a few dozen bytes on the
   stack, tables in PROGMEM, the compact model (`MINIMAL_FEATURES`).
5. **Model values come from physics, not curves or randomness.** Noise is
   sensor noise on top of a coherent state.
6. **Web handlers never touch the simulator** (ESP32 runs them on another
   task); the main loop publishes snapshots and applies queued commands. No
   `innerHTML`, no external resources.
7. **Track data keeps its attribution** (© OpenStreetMap contributors, ODbL).
8. Licence: MIT (other contributors hold copyright in parts).
