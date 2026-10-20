# HTTP API (ESP boards)

Served on port 80 next to the dashboard. JSON, no authentication: keep the
board on a network you trust. Values are published by the main loop every
100 ms (live), 500 ms (packet) and 1 s (status).

| Method | Path | |
|---|---|---|
| GET | `/api/status` | firmware, identity, platform, uptime, free heap, phase, track, Wi-Fi, protocol counters, settings |
| GET | `/api/live` | phase, lap timing, car (speed, gear, g, pedal, brake, position) and main ECU values |
| GET | `/api/packet` | `{"bytes":[…130…]}` — the realtime packet |
| GET | `/api/track` | track name, length, start-line coordinates, outline points (m east/north of the start line), attribution |
| GET | `/api/events` | Server-Sent Events: `live` (10 Hz) and `packet` (2 Hz), same documents as above |
| POST | `/api/control` | `action=start\|stop\|session\|pit\|blip` |
| POST | `/api/settings` | any of `pace` (0.5–1.05), `hotLaps` (1–50), `ambientC` (−20–45), `startCoolantC` (−20–95), `autoStart`, `launchControl`, `sensorNoise` (`1`/`0`) |
| GET | `/api/wifi/scan` | starts a scan; poll until `scanning` is false, then `networks: [{ssid, rssi, secure}]` |
| POST | `/api/wifi` | `ssid`, `password` → saved, restarts into station mode |
| POST | `/api/wifi/reset` | forgets the network, restarts as the access point |

Example `live` document:

```json
{"uptime":312,"phase":"hot lap",
 "lap":{"n":3,"time":47.21,"last":122.05,"best":121.84,"dist":1412.6},
 "car":{"kmh":118.4,"gear":3,"latG":0.12,"longG":0.21,"brake":0,"pedal":100.0,"x":-412.3,"y":388.0},
 "ecu":{"rpm":5782,"map":99,"tps":100.0,"clt":93,"iat":24,"afr":12.4,"afrTarget":12.9,"adv":26,"pw":6.38,
        "bat":14.2,"oil":61,"fuelP":43,"ego":100,"gammaE":100,"ve":91,"idle":45,"idleTarget":850,"oilC":97,
        "engine":1,"status1":15,"status2":128,"status4":128}}
```

# Code

| Component | |
|---|---|
| `EngineSimulator` | physics and ECU logic; `begin()`, `update()` (call often; advances in `SIM_STEP_MS` steps), `command()`, `settings()`, `getStatus()`, `telemetry()`, `phase()` |
| `SpeeduinoProtocol` | `poll()` handles one command from an `ISerialInterface`; `stats()`; `wirePacing` |
| `track::` | envelope, heading, curvature, position at a lap distance |
| `EngineStatus` | the packed 130-byte packet with unit-correct setters |
| `WebInterface` | Wi-Fi, assets, API (ESP only) |
| `Snapshot` | the JSON documents |
| `ITimeProvider`, `IRandomProvider`, `ISerialInterface` | platform seams (Arduino, native, tests) |
