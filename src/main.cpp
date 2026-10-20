/**
 * @file main.cpp
 * @brief Firmware entry point: a Speeduino on the serial line (or TCP port
 *        5000 on Wi-Fi builds with ENABLE_WIFI_SERIAL), driving laps of
 *        Biķernieki, with a web dashboard on ESP boards.
 *
 * When the protocol owns the USB/UART port nothing else is printed on it —
 * a real Speeduino sends no banner, and stray text would corrupt the first
 * reply a client reads.
 */
#if defined(ARDUINO) && !defined(UNIT_TEST)

#include <Arduino.h>
#include "Config.h"
#include "EngineSimulator.h"
#include "PlatformAdapters.h"
#include "SpeeduinoProtocol.h"

#if defined(SERIAL_PROTOCOL_ON_USB)
  #undef ENABLE_WIFI_SERIAL  // protocol on USB, no logs
#endif
#if defined(ENABLE_WIFI) && defined(ENABLE_WIFI_SERIAL)
  #include "WiFiSerialAdapter.h"
  #define LOG(...) Serial.printf(__VA_ARGS__)
#else
  #define LOG(...) ((void)0)
#endif
#ifdef ENABLE_WEB_INTERFACE
  #include "WebInterface.h"
#endif

static ArduinoTimeProvider timeSource;
static ArduinoRandomProvider rng;
#if defined(ENABLE_WIFI) && defined(ENABLE_WIFI_SERIAL)
static WiFiSerialAdapter line(WIFI_SERIAL_PORT);
static const char* TRANSPORT = "tcp:5000";
#else
static ArduinoSerialAdapter line(Serial);
static const char* TRANSPORT = "serial";
#endif
static EngineSimulator sim(&timeSource, &rng);
static SpeeduinoProtocol protocol(&line, &sim, &timeSource);
#ifdef ENABLE_WEB_INTERFACE
static WebInterface web(&sim, &protocol, TRANSPORT);
#endif

#ifdef LED_BUILTIN
static uint32_t ledOffAt = 0;
#endif

void setup() {
    Serial.begin(SERIAL_BAUD_RATE);
#ifdef LED_BUILTIN
    pinMode(LED_BUILTIN, OUTPUT);
#endif
    sim.begin();
    protocol.begin();
#ifdef ENABLE_WEB_INTERFACE
    web.begin();
    LOG("speeduino-serial-sim %s, %s at http://%s/\n", FIRMWARE_VERSION,
        web.accessPoint() ? "access point " WIFI_AP_SSID : "station", web.ip().toString().c_str());
#endif
#if defined(ENABLE_WIFI) && defined(ENABLE_WIFI_SERIAL)
    line.begin(0);
    LOG("Speeduino protocol on tcp port %u\n", WIFI_SERIAL_PORT);
#endif
    (void)TRANSPORT;
}

void loop() {
    sim.update();
    if (protocol.poll()) {
#ifdef LED_BUILTIN
        digitalWrite(LED_BUILTIN, HIGH);
        ledOffAt = millis() + 30;
#endif
    }
#ifdef LED_BUILTIN
    if (ledOffAt && (int32_t)(millis() - ledOffAt) >= 0) {
        digitalWrite(LED_BUILTIN, LOW);
        ledOffAt = 0;
    }
#endif
#ifdef ENABLE_WEB_INTERFACE
  #if defined(ENABLE_WIFI_SERIAL)
    web.loop(line.hasClient());
  #else
    web.loop(protocol.stats().commands > 0 && millis() - protocol.stats().lastCommandMs < 3000);
  #endif
#endif
#ifdef ESP8266
    yield();
#endif
}

#endif
