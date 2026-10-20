---
name: protocol-fidelity
description: Change what speeduino-serial-sim sends — packet fields, units, status bits, command replies, identity strings, reply timing — so it stays byte-compatible with a real Speeduino 202501.6.
---

# Protocol fidelity

## Source of truth

Speeduino release 202501.6 on GitHub (`speeduino/speeduino`, tag `202501.6`):

| Question | File |
|---|---|
| Which byte holds what | `speeduino/logger.cpp` `getTSLogEntry()` |
| Its unit | `globals.h` (comments can be stale) and the code that computes it (`sensors.cpp`, `speeduino.ino`, `corrections.cpp`); `reference/speeduino.ini` output channel scales confirm |
| Command replies | `comms.cpp` (framed), `comms_legacy.cpp` (legacy) |
| Bit meanings | `globals.h` `BIT_*` defines |

Known traps: TPS is 0.5 % steps since 202201 (`map(…, 0, 200)`), PW and dwell
are µs, `d` CRC is big-endian, `n` no longer exists, `S` zeroes `secl`.

## Changing a field

1. Add or fix the setter in `include/EngineStatus.h` (physical units in).
2. Set it in `EngineSimulator::stepEcu()` from model state.
3. Add the field to the dashboard decoder (`web/app.js` `PACKET`) with the same scale.
4. Pin the encoding in `test/test_native` and, where the CORE4 capture
   (`test/fixtures/core4-202501-engine-off.bin`) covers it, compare.
5. docs/PROTOCOL.md table.

## Changing a reply

Mirror the firmware handler, including return codes (`rc::`). Stream with
`frameBegin/frameByte/frameEnd` (no response buffers: AVR). Add a framed and,
if it exists, a legacy test. Keep `pace()` in the path (it is applied by
`frameBegin`, `sendRaw` and `sendRealtime`).

Clients to check after a change: ecu-to-mqtt (`src/protocol/speeduino.rs`
decodes the same units) and TunerStudio with the 202501 INI.
