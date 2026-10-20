/**
 * @file WebInterface.cpp
 * @brief Wi-Fi, embedded dashboard (web/, gzip in flash) and the HTTP API.
 *
 *   GET  /api/status      device, Wi-Fi, protocol counters, settings
 *   GET  /api/live        lap, car and main ECU values
 *   GET  /api/packet      the 130 realtime bytes
 *   GET  /api/track       outline of the circuit
 *   GET  /api/events      Server-Sent Events "live" (10 Hz) and "packet" (2 Hz)
 *   POST /api/control     action=start|stop|session|pit|blip
 *   POST /api/settings    pace, hotLaps, ambientC, startCoolantC, autoStart, launchControl, sensorNoise
 *   GET  /api/wifi/scan   networks (starts a scan, poll until "scanning" is false)
 *   POST /api/wifi        ssid, password → saved, restart into station mode
 *   POST /api/wifi/reset  forget credentials, restart as access point
 */
#ifdef ENABLE_WEB_INTERFACE

#include "WebInterface.h"
#include <string.h>
#include "Snapshot.h"
#include "WebAssets.h"

#ifdef ESP32
  #include <ESPmDNS.h>
  #include <Preferences.h>
#else
  #include <EEPROM.h>
  #include <ESP8266mDNS.h>
#endif

namespace {

#ifdef ESP32
const char* PLATFORM = "ESP32";
#else
const char* PLATFORM = "ESP8266";
#endif

#ifdef ESP8266
struct StoredCredentials {
    uint32_t magic;
    char ssid[33];
    char password[65];
};
constexpr uint32_t CREDENTIALS_MAGIC = 0x53504431;  // "SPD1"
#endif

bool formFloat(AsyncWebServerRequest* r, const char* name, float lo, float hi, float& out) {
    if (!r->hasParam(name, true)) return false;
    float v = r->getParam(name, true)->value().toFloat();
    if (v < lo || v > hi) return false;
    out = v;
    return true;
}

bool formBool(AsyncWebServerRequest* r, const char* name, bool& out) {
    if (!r->hasParam(name, true)) return false;
    String v = r->getParam(name, true)->value();
    out = v == "1" || v == "true" || v == "on";
    return true;
}

void securityHeaders(AsyncWebServerResponse* res) {
    res->addHeader("Cache-Control", "no-store");
    res->addHeader("X-Content-Type-Options", "nosniff");
}

}  // namespace

WebInterface::WebInterface(EngineSimulator* s, SpeeduinoProtocol* p, const char* transport)
    : sim(s), protocol(p), serialTransport(transport), server(WEB_SERVER_PORT), events("/api/events") {
    live[0] = status[0] = packet[0] = 0;
}

void WebInterface::enter() {
#ifdef ESP32
    portENTER_CRITICAL(&lock);
#endif
}
void WebInterface::leave() {
#ifdef ESP32
    portEXIT_CRITICAL(&lock);
#endif
}

IPAddress WebInterface::ip() const { return apMode ? WiFi.softAPIP() : WiFi.localIP(); }

// ---------------------------------------------------------------- Wi-Fi

void WebInterface::loadCredentials() {
#ifdef ESP32
    Preferences prefs;
    prefs.begin("wifi", true);
    prefs.getString("ssid", ssid, sizeof(ssid));
    prefs.getString("pass", password, sizeof(password));
    prefs.end();
    if (!ssid[0]) {
        // Station settings saved by firmware 2.x ("wifi-config"): keep using them.
        Preferences old;
        if (old.begin("wifi-config", true)) {
            if (old.getBool("useSTA", false) && old.getBool("configured", false)) {
                old.getString("staSSID", ssid, sizeof(ssid));
                old.getString("staPass", password, sizeof(password));
            }
            old.end();
        }
    }
#else
    StoredCredentials c;
    EEPROM.begin(sizeof(c));
    EEPROM.get(0, c);
    EEPROM.end();
    if (c.magic == CREDENTIALS_MAGIC) {
        c.ssid[sizeof(c.ssid) - 1] = 0;
        c.password[sizeof(c.password) - 1] = 0;
        strcpy(ssid, c.ssid);
        strcpy(password, c.password);
    }
#endif
}

void WebInterface::saveCredentials(const char* s, const char* p) {
#ifdef ESP32
    Preferences prefs;
    prefs.begin("wifi", false);
    prefs.putString("ssid", s);
    prefs.putString("pass", p);
    prefs.end();
#else
    StoredCredentials c = {};
    c.magic = s[0] ? CREDENTIALS_MAGIC : 0;
    strncpy(c.ssid, s, sizeof(c.ssid) - 1);
    strncpy(c.password, p, sizeof(c.password) - 1);
    EEPROM.begin(sizeof(c));
    EEPROM.put(0, c);
    EEPROM.commit();
    EEPROM.end();
#endif
}

bool WebInterface::connectStation() {
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < WIFI_CONNECT_TIMEOUT_MS) {
        delay(100);
        sim->update();  // keep the engine model running while we wait
        protocol->poll();
    }
    return WiFi.status() == WL_CONNECTED;
}

void WebInterface::startAccessPoint() {
    WiFi.disconnect(true);
    WiFi.mode(WIFI_AP);
    WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASSWORD);
}

void WebInterface::begin() {
    loadCredentials();
    apMode = !(ssid[0] && connectStation());
    if (apMode) startAccessPoint();
#ifdef ESP32
    WiFi.setSleep(false);  // lower latency for the serial socket and SSE
#endif
    if (MDNS.begin(MDNS_HOSTNAME)) MDNS.addService("http", "tcp", WEB_SERVER_PORT);

#ifdef TRACK_OUTLINE
    const size_t trackCap = 4096;
    track = (char*)malloc(trackCap);
    if (track && !trackJson(track, trackCap)) strcpy(track, "{}");
#endif
    routes();
    server.addHandler(&events);
    server.begin();
}

// ---------------------------------------------------------------- routes

void WebInterface::sendBuffer(AsyncWebServerRequest* r, const char* buf) {
    enter();
    String body(buf);
    leave();
    AsyncWebServerResponse* res = r->beginResponse(200, "application/json", body.length() ? body : String("{}"));
    securityHeaders(res);
    r->send(res);
}

void WebInterface::queue(const Pending& p) {
    enter();
    pending = p;
    leave();
}

static void asset(AsyncWebServer& server, const char* path, const char* type, const uint8_t* data, size_t len) {
    server.on(path, HTTP_GET, [type, data, len](AsyncWebServerRequest* r) {
        AsyncWebServerResponse* res = r->beginResponse(200, type, data, len);
        res->addHeader("Content-Encoding", "gzip");
        res->addHeader("Cache-Control", "no-cache");
        res->addHeader("X-Content-Type-Options", "nosniff");
        res->addHeader("Content-Security-Policy",
                       "default-src 'none'; script-src 'self'; style-src 'self'; img-src 'self' data:; "
                       "connect-src 'self'; base-uri 'none'; form-action 'none'; frame-ancestors 'none'");
        r->send(res);
    });
}

void WebInterface::routes() {
    asset(server, "/", "text/html; charset=utf-8", WEB_INDEX_HTML, sizeof(WEB_INDEX_HTML));
    asset(server, "/app.css", "text/css", WEB_APP_CSS, sizeof(WEB_APP_CSS));
    asset(server, "/app.js", "application/javascript", WEB_APP_JS, sizeof(WEB_APP_JS));
    asset(server, "/icon.svg", "image/svg+xml", WEB_ICON_SVG, sizeof(WEB_ICON_SVG));

    server.on("/api/status", HTTP_GET, [this](AsyncWebServerRequest* r) { sendBuffer(r, status); });
    server.on("/api/live", HTTP_GET, [this](AsyncWebServerRequest* r) { sendBuffer(r, live); });
    server.on("/api/packet", HTTP_GET, [this](AsyncWebServerRequest* r) { sendBuffer(r, packet); });
    server.on("/api/track", HTTP_GET, [this](AsyncWebServerRequest* r) {
        AsyncWebServerResponse* res = r->beginResponse(200, "application/json", track ? track : "{}");
        securityHeaders(res);
        r->send(res);
    });

    server.on("/api/control", HTTP_POST, [this](AsyncWebServerRequest* r) {
        if (!r->hasParam("action", true)) return r->send(400, "application/json", "{\"error\":\"action required\"}");
        String a = r->getParam("action", true)->value();
        Pending p;
        p.kind = 1;
        if (a == "start") p.command = Command::StartEngine;
        else if (a == "stop") p.command = Command::StopEngine;
        else if (a == "session") p.command = Command::StartSession;
        else if (a == "pit") p.command = Command::Pit;
        else if (a == "blip") p.command = Command::Blip;
        else return r->send(400, "application/json", "{\"error\":\"unknown action\"}");
        queue(p);
        r->send(200, "application/json", "{\"ok\":true}");
    });

    server.on("/api/settings", HTTP_POST, [this](AsyncWebServerRequest* r) {
        Pending p;
        p.kind = 2;
        p.settings = sim->settings();  // a copy of plain values; applied in loop()
        float laps = p.settings.hotLaps;
        formFloat(r, "pace", 0.5f, 1.05f, p.settings.pace);
        if (formFloat(r, "hotLaps", 1, 50, laps)) p.settings.hotLaps = (uint8_t)laps;
        formFloat(r, "ambientC", -20, 45, p.settings.ambientC);
        formFloat(r, "startCoolantC", -20, 95, p.settings.startCoolantC);
        formBool(r, "autoStart", p.settings.autoStart);
        formBool(r, "launchControl", p.settings.launchControl);
        formBool(r, "sensorNoise", p.settings.sensorNoise);
        queue(p);
        r->send(200, "application/json", "{\"ok\":true}");
    });

    server.on("/api/wifi/scan", HTTP_GET, [](AsyncWebServerRequest* r) {
        int n = WiFi.scanComplete();
        if (n == WIFI_SCAN_FAILED) {
            WiFi.scanNetworks(true);
            return r->send(200, "application/json", "{\"scanning\":true,\"networks\":[]}");
        }
        if (n == WIFI_SCAN_RUNNING) return r->send(200, "application/json", "{\"scanning\":true,\"networks\":[]}");
        static char buf[1600];
        JsonWriter j(buf, sizeof(buf));
        j.open().boolean("scanning", false).openArray("networks");
        for (int i = 0; i < n && i < 20; i++) {
            j.open()
                .str("ssid", WiFi.SSID(i).c_str())
                .integer("rssi", WiFi.RSSI(i))
#ifdef ESP32
                .boolean("secure", WiFi.encryptionType(i) != WIFI_AUTH_OPEN)
#else
                .boolean("secure", WiFi.encryptionType(i) != ENC_TYPE_NONE)
#endif
                .close();
        }
        j.closeArray().close();
        WiFi.scanDelete();
        r->send(200, "application/json", j.overflow() ? "{\"scanning\":false,\"networks\":[]}" : buf);
    });

    server.on("/api/wifi", HTTP_POST, [this](AsyncWebServerRequest* r) {
        if (!r->hasParam("ssid", true)) return r->send(400, "application/json", "{\"error\":\"ssid required\"}");
        String s = r->getParam("ssid", true)->value();
        String p = r->hasParam("password", true) ? r->getParam("password", true)->value() : String();
        if (s.length() == 0 || s.length() > 32 || p.length() > 64)
            return r->send(400, "application/json", "{\"error\":\"ssid 1-32, password up to 64 characters\"}");
        saveCredentials(s.c_str(), p.c_str());
        Pending q;
        q.restart = true;
        queue(q);
        r->send(200, "application/json", "{\"ok\":true,\"restarting\":true}");
    });

    server.on("/api/wifi/reset", HTTP_POST, [this](AsyncWebServerRequest* r) {
        saveCredentials("", "");
        Pending q;
        q.restart = true;
        queue(q);
        r->send(200, "application/json", "{\"ok\":true,\"restarting\":true}");
    });

    server.onNotFound([](AsyncWebServerRequest* r) { r->send(404, "text/plain", "not found"); });
}

// ---------------------------------------------------------------- loop

void WebInterface::loop(bool serialClient) {
    uint32_t now = millis();

    // Apply what the web asked for, on the main loop.
    enter();
    Pending p = pending;
    pending = Pending();
    leave();
    if (p.kind == 1) sim->command(p.command);
    if (p.kind == 2) sim->settings() = p.settings;
    if (p.restart && !restartPending) {
        restartPending = true;
        restartAt = now + 800;  // let the response go out
    }
    if (restartPending && (int32_t)(now - restartAt) >= 0) ESP.restart();

    static char scratch[1536];
    if (now - lastLive >= 100) {
        lastLive = now;
        size_t n = liveJson(scratch, sizeof(live), *sim);
        if (n) {
            enter();
            memcpy(live, scratch, n + 1);
            leave();
            if (events.count()) events.send(scratch, "live", now);
        }
    }
    if (now - lastPacket >= 500) {
        lastPacket = now;
        size_t n = packetJson(scratch, sizeof(packet), *sim);
        if (n) {
            enter();
            memcpy(packet, scratch, n + 1);
            leave();
            if (events.count()) events.send(scratch, "packet", now);
        }
    }
    if (now - lastStatus >= 1000) {
        lastStatus = now;
        DeviceInfo d;
        d.platform = PLATFORM;
        d.freeHeap = ESP.getFreeHeap();
        d.wifiMode = apMode ? "access point" : "station";
        String addr = ip().toString();
        String net = apMode ? String(WIFI_AP_SSID) : WiFi.SSID();
        d.ip = addr.c_str();
        d.ssid = net.c_str();
        d.rssi = apMode ? 0 : WiFi.RSSI();
        d.streamClients = (uint8_t)events.count();
        d.serialClient = serialClient;
        d.serialTransport = serialTransport;
        d.nowMs = now;
        size_t n = statusJson(scratch, sizeof(status), *sim, protocol->stats(), d);
        if (n) {
            enter();
            memcpy(status, scratch, n + 1);
            leave();
        }
    }
#ifdef ESP8266
    MDNS.update();
#endif
}

#endif
