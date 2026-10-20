---
name: lap-model
description: Change how the simulated car and engine behave in speeduino-serial-sim — driver, gearbox, engine torque, manifold, thermal model, Speeduino fuel/ignition/idle logic, sessions — or regenerate the track.
---

# Lap model

Everything lives in `src/EngineSimulator.cpp`, constants in `include/Vehicle.h`.
`stepPhysics()` runs driver → engine/car → thermal → ECU every `SIM_STEP_MS`.
The ECU layer only sees sensed values and sets the packet; physics never
writes packet bytes.

## Calibration targets (keep them; tests check most)

| | |
|---|---|
| Hot lap at pace 1.0 | ~2:02 (110–134 s in tests) |
| Idle | 850 rpm hot, ~35 kPa, PW ~2.3 ms |
| WOT | 98–100 kPa, PW 6–7 ms, AFR ~12–13 |
| Overrun | ~15 kPa, DFCO after 0.3 s |
| Coolant | 85–98 °C on hot laps, fan cycles at idle after a run |
| Max rpm | < 7350 (limiters at 7000/7200) |

Tune with the desktop build: `pio run -e native && .pio/build/native/program
--speed 25` prints phase, lap, speed, gear, rpm, TPS, MAP, CLT, AFR and PW
every second. Then run `pio test -e native`.

## Rules

- Prefer a physical cause over a correction factor (the fuel film, not an
  AFR offset; the radiator, not a temperature clamp).
- Keep the compact path (`SIM_DETAILED == 0`) working: AVR uses the same
  driver and engine with fewer features. Build `uno` after changes.
- New settings: `SimSettings` default, `/api/settings` range check in
  `WebInterface.cpp`, the Control form, the native CLI, README.

## Track

`scripts/gen_track.py <outline.json> include/TrackData.h` from a closed
outline (`data/tracks/*.json`, with attribution and licence). The envelope
uses `LAT_G` and `BRAKE_G` in the script; the live driver scales it by pace.
