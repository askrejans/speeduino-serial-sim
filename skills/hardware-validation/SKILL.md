---
name: hardware-validation
description: Flash and test speeduino-serial-sim on boards, read it with real clients (ecu-to-mqtt, TunerStudio), and compare against a real Speeduino without writing to it.
---

# Hardware validation

## Flash

`pio device list` (ESP32-S3 native USB is 303A:1001). Generic S3 modules use
`esp32s3-devkit` (Wi-Fi protocol) or `esp32s3-devkit-usb` (protocol on USB);
the XIAO uses `esp32s3`. `pio run -e <env> -t upload --upload-port <port>`.
Wi-Fi credentials survive reflashing (NVS). Never enter someone's Wi-Fi
password for them: they join the board through its access point page.

## Check with ecu-to-mqtt

```bash
ECU_TO_MQTT_MQTT_ENABLED=false ECU_TO_MQTT_HTTP_ENABLED=true ecu-to-mqtt --headless
curl -s http://127.0.0.1:9091/health      # 50 Hz, ~14 ms latency expected
curl -s http://127.0.0.1:9091/api/v1/live # channels, packet, flags
```

Expect detection by signature, the CORE4's status bits (allow legacy comms,
TS on secondary), and values that follow the session phase.

## Comparing with a real Speeduino

Only `Q`, `S` and `A` (and `r` 0x30) may be sent to a real controller; they
change nothing. Do not send `M`, `b`, `B`, `t`, `U` or page commands to a car's
ECU. Capture with a short script, save as `test/fixtures/<board>-<firmware>-<state>.bin`
and compare in a test. Opening the port resets Arduino-based boards (DTR).

Stop gateways and monitors you started; free the serial ports.
