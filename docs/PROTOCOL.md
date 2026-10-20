# Speeduino protocol as the simulator speaks it

Reference: Speeduino release **202501.6** — `speeduino/comms.cpp`,
`comms_legacy.cpp`, `logger.cpp` (`getTSLogEntry()`), `globals.h`,
`sensors.cpp`, and the release's `reference/speeduino.ini`. Identity strings
and reply timing were checked against a DIY-EFI CORE4 running that release.
Older wiki pages (Secondary Serial IO interface, 2020–21) describe the
secondary port and predate the 0.5 % TPS change of 202201; where they differ,
the firmware source wins.

Line: 115200 baud, 8N1. Replies are paced to that speed on USB CDC and TCP.

## Legacy (single byte)

| Command | Reply |
|---|---|
| `A` | 130 bytes, the realtime packet (no echo on the primary port) |
| `Q` | `speeduino 202501` |
| `S` | `Speeduino 2025.01.6`; zeroes `secl`, clears status5 bit 5 (TunerStudio on secondary) |
| `F` | `002` (serial protocol version) |
| `C` | `0x01` |
| `c` | loops per second, u16 LE |
| `m` | free RAM, u16 LE |
| `d` 0x00 page | page CRC32, big-endian |
| `r` canId 0x30 offset(LE) length(LE) | `length` packet bytes from `offset` |
| anything else | ignored, as the firmware does for this simulator's unsupported commands |

## Framed (TunerStudio)

Request and reply: `[u16 BE length][payload][u32 BE CRC32 of payload]`.
Replies start with a return code: `0x00` OK, `0x04` burn OK, `0x80` timeout,
`0x82` CRC error, `0x83` unknown command, `0x84` range error.

| Payload | Reply |
|---|---|
| `Q` | OK + `speeduino 202501` |
| `S` | OK + `Speeduino 2025.01.6` (and the `secl` reset) |
| `F` | OK + `002` |
| `C` | OK + `0xFF` |
| `f` | OK, protocol 2, blocking factor 121, table blocking factor 64 (AVR/STM32 values, as the CORE4) |
| `A` | OK + 130 packet bytes |
| `r` id 0x30 offset(LE) length(LE) | OK + packet bytes |
| `r` id 0x0F … | OK + signature |
| `p` id page offset(LE) length(LE) | OK + page bytes |
| `M` id page offset(LE) length(LE) data… | OK; stored on ESP/native, dropped on AVR |
| `d` id page | OK + page CRC32, big-endian |
| `k` | OK + 4 zero bytes (calibration CRCs) |
| `b`, `B` | burn OK |
| `E`, `I` | OK + 0 (CAN ID) |
| `H h J j O o X x` | OK (no loggers) |
| `t` | OK (calibration upload not applied) |

Pages 1–15 have the INI sizes 128, 288, 288, 128, 288, 128, 240, 384, 192,
192, 288, 192, 128, 288 and 256 bytes and read as zeros until written.

## The realtime packet

130 bytes in `getTSLogEntry()` order. Multi-byte values are little-endian.
The full list is in [include/EngineStatus.h](../include/EngineStatus.h); the
fields whose units are commonly misread:

| Bytes | Field | Unit on the wire |
|---|---|---|
| 4–5 | MAP | kPa |
| 6, 7, 116 | IAT, CLT, fuel temperature | °C + 40 |
| 9 | battery | V × 10 |
| 10, 40, 21 | O2, O2 #2, AFR target | AFR × 10 |
| 16 | AE amount | % ÷ 2 |
| 17–18 | total correction (gammaE) | % |
| 22–23 | TPSdot | %/s, signed |
| 24, 118, 119 | advance | degrees, signed |
| 25 | TPS | 0.5 % steps (0–200) |
| 30, 31 | boost target, boost duty | kPa ÷ 2, % |
| 33–34 | RPMdot | rpm/s, signed |
| 76–83 | PW1–PW4 | µs |
| 84 | status3 | bits 5–7: squirts per cycle |
| 86–89 | fuel load, ignition load | signed (kPa for speed density) |
| 90–91, 125–126 | dwell, measured dwell | µs |
| 92 | closed-loop idle target | rpm ÷ 10 |
| 93–94 | MAPdot | kPa/s, signed |
| 95–98, 111–114 | VVT angle/target/duty | 0.5 steps |
| 104–105 | VSS | km/h |
| 107, 108 | fuel pressure, oil pressure | PSI |
| 123 | fan duty | 0.5 % steps |

### Status bits (bit 0 first)

| Byte | Group | Bits |
|---|---|---|
| 2 | engine | running, cranking, ASE, warm-up, TPS accel, TPS decel, MAP accel, MAP decel |
| 1 | status1 | inj1–inj4, DFCO, boost cut fuel, tooth log 1, tooth log 2 |
| 32 | status2 | hard launch, soft launch, hard limit, soft limit, boost cut spark, error, idle control, sync |
| 84 | status3 | reset lock, nitrous, fuel 2, VSS refresh, half sync, squirts (5–7) |
| 110 | status4 | WMI empty, VVT1 error, VVT2 error, fan, burn pending, staging, comms compat, allow legacy comms |
| 127 | status5 | flat shift hard, flat shift soft, spark 2, knock active, knock pulse, TS on secondary |

The simulator sets running/cranking/ASE/warm-up/TPS accel/decel, the injector
bits with their duty cycle, DFCO, soft launch, both limiters, idle control,
sync, squirts = 2, fan, allow-legacy-comms (as the CORE4) and TS-on-secondary
until `S` arrives.
