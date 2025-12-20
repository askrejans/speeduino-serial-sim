/**
 * @file WebInterface.cpp
 * @brief Implementation of web interface for ESP32/ESP8266
 */

#ifdef ENABLE_WEB_INTERFACE

#include "WebInterface.h"

WebInterface::WebInterface(EngineSimulator* simulator, SpeeduinoProtocol* protocol)
    : server(nullptr)
    , simulator(simulator)
    , protocol(protocol)
    , wifiConnected(false)
{
}

WebInterface::~WebInterface() {
    if (server != nullptr) {
        delete server;
    }
}

bool WebInterface::begin(bool apMode) {
    if (apMode) {
        setupWiFiAP();
    } else {
        setupWiFiStation();
    }
    
    if (!wifiConnected) {
        return false;
    }
    
    // Initialize mDNS
    if (!MDNS.begin(MDNS_HOSTNAME)) {
        Serial.println("Error setting up mDNS");
    } else {
        Serial.println("mDNS responder started");
        MDNS.addService("http", "tcp", WEB_SERVER_PORT);
    }
    
    // Create web server
    server = new AsyncWebServer(WEB_SERVER_PORT);
    
    // Setup routes
    setupRoutes();
    
    // Start server
    server->begin();
    
    Serial.print("Web interface started at http://");
    Serial.print(ipAddress.toString());
    Serial.println("/");
    
    return true;
}

void WebInterface::setupWiFiAP() {
    Serial.println("Starting WiFi Access Point...");
    
    WiFi.mode(WIFI_AP);
    bool success = WiFi.softAP(WIFI_SSID, WIFI_PASSWORD);
    
    if (success) {
        ipAddress = WiFi.softAPIP();
        wifiConnected = true;
        Serial.print("AP started. IP: ");
        Serial.println(ipAddress);
    } else {
        Serial.println("AP failed to start");
        wifiConnected = false;
    }
}

void WebInterface::setupWiFiStation() {
    Serial.println("Connecting to WiFi...");
    
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    
    uint32_t startTime = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startTime < WIFI_TIMEOUT_MS) {
        delay(500);
        Serial.print(".");
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        ipAddress = WiFi.localIP();
        wifiConnected = true;
        Serial.println("\nWiFi connected");
        Serial.print("IP: ");
        Serial.println(ipAddress);
    } else {
        Serial.println("\nWiFi connection failed");
        wifiConnected = false;
    }
}

void WebInterface::update() {
    // Handle mDNS
    #ifdef ESP8266
        MDNS.update();
    #endif
    
    // Check WiFi connection
    if (!wifiConnected) {
        // Attempt reconnection
        if (WiFi.status() == WL_CONNECTED) {
            wifiConnected = true;
            ipAddress = WiFi.localIP();
        }
    }
}

void WebInterface::setupRoutes() {
    // Home page
    server->on("/", HTTP_GET, [this](AsyncWebServerRequest* request) {
        handleRoot(request);
    });
    
    // API endpoints
    server->on("/api/status", HTTP_GET, [this](AsyncWebServerRequest* request) {
        handleStatus(request);
    });
    
    server->on("/api/realtime", HTTP_GET, [this](AsyncWebServerRequest* request) {
        handleRealtimeData(request);
    });
    
    server->on("/api/statistics", HTTP_GET, [this](AsyncWebServerRequest* request) {
        handleStatistics(request);
    });
    
    server->on("/api/setmode", HTTP_POST, [this](AsyncWebServerRequest* request) {
        handleSetMode(request);
    });
    
    // 404 handler
    server->onNotFound([this](AsyncWebServerRequest* request) {
        handleNotFound(request);
    });
}

void WebInterface::handleRoot(AsyncWebServerRequest* request) {
    String html = generateHomePage();
    request->send(200, "text/html", html);
}

void WebInterface::handleStatus(AsyncWebServerRequest* request) {
    String json = getStatusJSON();
    request->send(200, "application/json", json);
}

void WebInterface::handleRealtimeData(AsyncWebServerRequest* request) {
    String json = getRealtimeJSON();
    request->send(200, "application/json", json);
}

void WebInterface::handleStatistics(AsyncWebServerRequest* request) {
    String json = getStatisticsJSON();
    request->send(200, "application/json", json);
}

void WebInterface::handleSetMode(AsyncWebServerRequest* request) {
    if (request->hasParam("mode", true)) {
        String modeStr = request->getParam("mode", true)->value();
        EngineMode newMode = EngineMode::IDLE;
        
        if (modeStr == "idle") newMode = EngineMode::IDLE;
        else if (modeStr == "light_load") newMode = EngineMode::LIGHT_LOAD;
        else if (modeStr == "acceleration") newMode = EngineMode::ACCELERATION;
        else if (modeStr == "high_rpm") newMode = EngineMode::HIGH_RPM;
        else if (modeStr == "wot") newMode = EngineMode::WOT;
        
        simulator->setMode(newMode);
        request->send(200, "application/json", "{\"success\":true}");
    } else {
        request->send(400, "application/json", "{\"success\":false,\"error\":\"Missing mode parameter\"}");
    }
}

void WebInterface::handleNotFound(AsyncWebServerRequest* request) {
    request->send(404, "text/plain", "Not found");
}

String WebInterface::generateHomePage() {
    String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Speeduino Simulator</title>
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body { font-family: Arial, sans-serif; background: #1a1a1a; color: #fff; padding: 20px; }
        .container { max-width: 1200px; margin: 0 auto; }
        h1 { color: #00ff88; margin-bottom: 20px; }
        .card { background: #2a2a2a; border-radius: 8px; padding: 20px; margin-bottom: 20px; }
        .gauge-container { display: grid; grid-template-columns: repeat(auto-fit, minmax(200px, 1fr)); gap: 20px; }
        .gauge { text-align: center; padding: 20px; background: #333; border-radius: 8px; }
        .gauge-value { font-size: 2em; color: #00ff88; font-weight: bold; }
        .gauge-label { color: #aaa; margin-top: 10px; }
        .controls { display: flex; gap: 10px; flex-wrap: wrap; }
        .btn { padding: 10px 20px; background: #00ff88; color: #000; border: none; border-radius: 4px; cursor: pointer; font-size: 1em; }
        .btn:hover { background: #00cc6a; }
        .status-indicator { display: inline-block; width: 12px; height: 12px; border-radius: 50%; margin-right: 8px; }
        .status-running { background: #00ff88; }
        .status-error { background: #ff4444; }
        .info { color: #aaa; line-height: 1.6; }
    </style>
</head>
<body>
    <div class="container">
        <h1>Speeduino Simulator</h1>
        
        <div class="card">
            <h2><span class="status-indicator status-running"></span>Status</h2>
            <div class="info">
                <p><strong>Mode:</strong> <span id="mode">Loading...</span></p>
                <p><strong>Runtime:</strong> <span id="runtime">0</span> seconds</p>
                <p><strong>Commands:</strong> <span id="commands">0</span></p>
                <p><strong>Firmware:</strong> )rawliteral" + String(FIRMWARE_VERSION) + R"rawliteral(</p>
            </div>
        </div>
        
        <div class="card">
            <h2>Real-time Data</h2>
            <div class="gauge-container">
                <div class="gauge">
                    <div class="gauge-value" id="rpm">0</div>
                    <div class="gauge-label">RPM</div>
                </div>
                <div class="gauge">
                    <div class="gauge-value" id="clt">0</div>
                    <div class="gauge-label">Coolant °C</div>
                </div>
                <div class="gauge">
                    <div class="gauge-value" id="map">0</div>
                    <div class="gauge-label">MAP kPa</div>
                </div>
                <div class="gauge">
                    <div class="gauge-value" id="tps">0</div>
                    <div class="gauge-label">TPS %</div>
                </div>
                <div class="gauge">
                    <div class="gauge-value" id="afr">0.0</div>
                    <div class="gauge-label">AFR</div>
                </div>
                <div class="gauge">
                    <div class="gauge-value" id="advance">0</div>
                    <div class="gauge-label">Timing °</div>
                </div>
            </div>
        </div>
        
        <div class="card">
            <h2>Controls</h2>
            <div class="controls">
                <button class="btn" onclick="setMode('idle')">Idle</button>
                <button class="btn" onclick="setMode('light_load')">Light Load</button>
                <button class="btn" onclick="setMode('acceleration')">Acceleration</button>
                <button class="btn" onclick="setMode('high_rpm')">High RPM</button>
                <button class="btn" onclick="setMode('wot')">WOT</button>
            </div>
        </div>
    </div>
    
    <script>
        function updateData() {
            fetch('/api/realtime')
                .then(r => r.json())
                .then(data => {
                    document.getElementById('rpm').textContent = data.rpm;
                    document.getElementById('clt').textContent = data.clt;
                    document.getElementById('map').textContent = data.map;
                    document.getElementById('tps').textContent = data.tps;
                    document.getElementById('afr').textContent = data.afr;
                    document.getElementById('advance').textContent = data.advance;
                });
            
            fetch('/api/statistics')
                .then(r => r.json())
                .then(data => {
                    document.getElementById('mode').textContent = data.mode;
                    document.getElementById('runtime').textContent = data.runtime;
                    document.getElementById('commands').textContent = data.commands;
                });
        }
        
        function setMode(mode) {
            fetch('/api/setmode', {
                method: 'POST',
                headers: {'Content-Type': 'application/x-www-form-urlencoded'},
                body: 'mode=' + mode
            }).then(() => updateData());
        }
        
        setInterval(updateData, 1000);
        updateData();
    </script>
</body>
</html>
)rawliteral";
    
    return html;
}

String WebInterface::getStatusJSON() {
    StaticJsonDocument<256> doc;
    
    const char* modeStr = "unknown";
    switch (simulator->getMode()) {
        case EngineMode::STARTUP: modeStr = "startup"; break;
        case EngineMode::WARMUP_IDLE: modeStr = "warmup_idle"; break;
        case EngineMode::IDLE: modeStr = "idle"; break;
        case EngineMode::LIGHT_LOAD: modeStr = "light_load"; break;
        case EngineMode::ACCELERATION: modeStr = "acceleration"; break;
        case EngineMode::HIGH_RPM: modeStr = "high_rpm"; break;
        case EngineMode::DECELERATION: modeStr = "deceleration"; break;
        case EngineMode::WOT: modeStr = "wot"; break;
    }
    
    doc["mode"] = modeStr;
    doc["runtime"] = simulator->getRuntime();
    doc["connected"] = wifiConnected;
    doc["ip"] = ipAddress.toString();
    
    String output;
    serializeJson(doc, output);
    return output;
}

String WebInterface::getRealtimeJSON() {
    const EngineStatus& status = simulator->getStatus();
    
    StaticJsonDocument<512> doc;
    
    doc["rpm"] = status.getRPM();
    doc["clt"] = status.getCoolantTemp();
    doc["iat"] = status.getIntakeTemp();
    doc["map"] = status.getMAP();
    doc["tps"] = status.tps;
    doc["afr"] = status.afrtarget / 10.0;
    doc["advance"] = status.advance;
    doc["pw"] = status.getPulseWidth() / 10.0;
    doc["battery"] = status.batteryv / 10.0;
    doc["ve"] = status.ve;
    
    String output;
    serializeJson(doc, output);
    return output;
}

String WebInterface::getStatisticsJSON() {
    StaticJsonDocument<256> doc;
    
    const char* modeStr = "unknown";
    switch (simulator->getMode()) {
        case EngineMode::STARTUP: modeStr = "Startup"; break;
        case EngineMode::WARMUP_IDLE: modeStr = "Warming Up"; break;
        case EngineMode::IDLE: modeStr = "Idle"; break;
        case EngineMode::LIGHT_LOAD: modeStr = "Light Load"; break;
        case EngineMode::ACCELERATION: modeStr = "Accelerating"; break;
        case EngineMode::HIGH_RPM: modeStr = "High RPM"; break;
        case EngineMode::DECELERATION: modeStr = "Decelerating"; break;
        case EngineMode::WOT: modeStr = "Wide Open Throttle"; break;
    }
    
    doc["mode"] = modeStr;
    doc["runtime"] = simulator->getRuntime();
    doc["commands"] = protocol->getCommandCount();
    doc["errors"] = protocol->getErrorCount();
    
    String output;
    serializeJson(doc, output);
    return output;
}

#endif // ENABLE_WEB_INTERFACE
