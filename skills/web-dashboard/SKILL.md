---
name: web-dashboard
description: Change the speeduino-serial-sim web dashboard (web/) or its HTTP API on ESP boards while keeping it safe, small and off the simulator's thread.
---

# Web dashboard

`web/index.html`, `app.css`, `app.js`, `icon.svg` are gzip-embedded into
`include/WebAssets.h` by `scripts/embed_web.py` before every ESP build. No
bundler, no framework, no external resources. The style is shared with
gps-to-mqtt and ecu-to-mqtt (`app.css`); keep both light and dark themes working.

## Rules

- DOM with `el()`/`textContent` only; never `innerHTML` (SSIDs and anything
  from the network are untrusted). The CSP in `WebInterface.cpp` allows
  `'self'` only.
- Data: `api/events` (`live` 10 Hz, `packet` 2 Hz), `api/status` (2 s poll),
  `api/track` once. State changes are POST form bodies.
- Handlers in `WebInterface.cpp` run on the network task on ESP32: they copy
  published buffers under the lock and queue `Pending` work; the main loop
  applies it. Never call the simulator from a handler.
- Keep JSON under the buffer sizes (`live` 900, `status` 1536, `packet` 640
  bytes); `Snapshot` returns 0 on overflow and the test checks the sizes.
- ESP8266 has ~40 KB free heap with Wi-Fi up: avoid large `String`s.

## Verify

Flash an ESP build, open the dashboard (access point `SpeeduinoSim`,
http://192.168.4.1 or the station address), check every mode in both themes
and at phone width, and run a full session to see laps on the Track mode.
