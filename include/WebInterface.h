/**
 * @file WebInterface.h
 * @brief Wi-Fi (station with access-point fallback), web dashboard and HTTP
 *        API on ESP32 / ESP8266.
 *
 * Request handlers run on the network task (ESP32) and never touch the
 * simulator: the main loop publishes JSON snapshots into locked buffers and
 * applies queued commands.
 */
#pragma once

#ifdef ENABLE_WEB_INTERFACE

#include <Arduino.h>
#include "EngineSimulator.h"
#include "SpeeduinoProtocol.h"

#ifdef ESP32
  #include <AsyncTCP.h>
  #include <WiFi.h>
#elif defined(ESP8266)
  #include <ESP8266WiFi.h>
  #include <ESPAsyncTCP.h>
#endif
#include <ESPAsyncWebServer.h>

class WebInterface {
public:
    WebInterface(EngineSimulator* sim, SpeeduinoProtocol* protocol, const char* serialTransport);
    /// Start Wi-Fi and the server. Station mode with saved credentials, else
    /// an access point (SpeeduinoSim / speeduino123 at 192.168.4.1).
    void begin();
    /// Call every loop(): publishes snapshots, streams, applies commands.
    void loop(bool serialClient);
    bool accessPoint() const { return apMode; }
    IPAddress ip() const;

private:
    struct Pending {
        uint8_t kind = 0;   // 0 none, 1 command, 2 settings
        Command command = Command::StartEngine;
        SimSettings settings;
        bool restart = false;
    };

    EngineSimulator* sim;
    SpeeduinoProtocol* protocol;
    const char* serialTransport;
    AsyncWebServer server;
    AsyncEventSource events;
    bool apMode = true;
    bool restartPending = false;
    uint32_t restartAt = 0;
    uint32_t lastLive = 0, lastStatus = 0, lastPacket = 0;
    char ssid[33] = {};
    char password[65] = {};

    char live[900];
    char status[1536];
    char packet[640];
    char* track = nullptr;
    Pending pending;
#ifdef ESP32
    portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;
#endif

    void loadCredentials();
    void saveCredentials(const char* ssid, const char* pass);
    bool connectStation();
    void startAccessPoint();
    void routes();
    void sendBuffer(AsyncWebServerRequest* r, const char* buf);
    void queue(const Pending& p);
    void enter();
    void leave();
};

#endif
