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
    , isAPMode(true)
{
    // Initialize config with defaults
    memset(&config, 0, sizeof(config));
    config.useStationMode = false;
    config.isConfigured = false;
}

WebInterface::~WebInterface() {
    #ifdef ESP32
        preferences.end();
    #endif
    if (server != nullptr) {
        delete server;
    }
}

void WebInterface::loadConfig() {
    #ifdef ESP32
        preferences.begin("wifi-config", false);
        config.useStationMode = preferences.getBool("useSTA", false);
        config.isConfigured = preferences.getBool("configured", false);
        preferences.getString("staSSID", config.staSSID, sizeof(config.staSSID));
        preferences.getString("staPass", config.staPassword, sizeof(config.staPassword));
        preferences.end();
    #elif defined(ESP8266)
        EEPROM.begin(512);
        EEPROM.get(0, config);
        EEPROM.end();
        // Validate EEPROM data
        if (config.staSSID[0] == 0xFF) {
            config.isConfigured = false;
            config.useStationMode = false;
        }
    #endif
    
    Serial.println("WiFi Config Loaded:");
    Serial.print("  Configured: ");
    Serial.println(config.isConfigured ? "Yes" : "No");
    Serial.print("  Mode: ");
    Serial.println(config.useStationMode ? "Station" : "AP");
    if (config.isConfigured && config.staSSID[0] != 0) {
        Serial.print("  SSID: ");
        Serial.println(config.staSSID);
    }
}

void WebInterface::saveConfig() {
    #ifdef ESP32
        preferences.begin("wifi-config", false);
        preferences.putBool("useSTA", config.useStationMode);
        preferences.putBool("configured", config.isConfigured);
        preferences.putString("staSSID", config.staSSID);
        preferences.putString("staPass", config.staPassword);
        preferences.end();
    #elif defined(ESP8266)
        EEPROM.begin(512);
        EEPROM.put(0, config);
        EEPROM.commit();
        EEPROM.end();
    #endif
    Serial.println("WiFi config saved");
}

void WebInterface::resetConfig() {
    memset(&config, 0, sizeof(config));
    config.useStationMode = false;
    config.isConfigured = false;
    saveConfig();
    Serial.println("WiFi config reset to defaults");
}

bool WebInterface::begin() {
    Serial.println("WebInterface::begin() called");
    
    // Load saved configuration
    loadConfig();
    
    // Try station mode if configured
    if (config.isConfigured && config.useStationMode) {
        Serial.println("Attempting to connect to configured WiFi...");
        if (attemptStationConnection()) {
            isAPMode = false;
            wifiConnected = true;
            Serial.println("Station mode successful");
        } else {
            Serial.println("Station mode failed, falling back to AP mode");
            setupWiFiAP();
            isAPMode = true;
        }
    } else {
        // Start in AP mode
        Serial.println("Starting in AP mode (no station config)");
        setupWiFiAP();
        isAPMode = true;
    }
    
    if (!wifiConnected) {
        Serial.println("ERROR: WiFi not connected after initialization");
        return false;
    }
    
    Serial.println("WiFi connected, starting web server...");
    
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
    
    // Cleanly tear down any prior WiFi state to avoid
    // "addba response cb: ap bss deleted" errors from ESP-IDF
    WiFi.disconnect(true);
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);
    delay(100);
    
    WiFi.mode(WIFI_AP);
    bool success = WiFi.softAP(WIFI_SSID, WIFI_PASSWORD);
    
    if (success) {
        delay(100);  // Allow AP to stabilize
        ipAddress = WiFi.softAPIP();
        wifiConnected = true;
        Serial.print("AP started. IP: ");
        Serial.println(ipAddress);
    } else {
        Serial.println("AP failed to start");
        wifiConnected = false;
    }
}

bool WebInterface::setupWiFiStation() {
    Serial.println("Connecting to WiFi...");
    
    // Cleanly tear down any prior WiFi state
    WiFi.disconnect(true);
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);
    delay(100);
    
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
        return true;
    } else {
        Serial.println("\nWiFi connection failed");
        wifiConnected = false;
        return false;
    }
}

bool WebInterface::attemptStationConnection() {
    Serial.print("Connecting to: ");
    Serial.println(config.staSSID);
    
    // Cleanly tear down any prior WiFi state
    WiFi.disconnect(true);
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);
    delay(100);
    
    WiFi.mode(WIFI_STA);
    WiFi.begin(config.staSSID, config.staPassword);
    
    uint32_t startTime = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startTime < WIFI_TIMEOUT_MS) {
        delay(500);
        Serial.print(".");
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        ipAddress = WiFi.localIP();
        wifiConnected = true;
        Serial.println("\nConnected to WiFi!");
        Serial.print("IP: ");
        Serial.println(ipAddress);
        return true;
    } else {
        Serial.println("\nFailed to connect");
        wifiConnected = false;
        return false;
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
    
    // WiFi configuration page
    server->on("/wifi", HTTP_GET, [this](AsyncWebServerRequest* request) {
        handleWiFiConfig(request);
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
    
    // WiFi configuration endpoints
    server->on("/api/wifi/scan", HTTP_GET, [this](AsyncWebServerRequest* request) {
        handleWiFiScan(request);
    });
    
    server->on("/api/wifi/save", HTTP_POST, [this](AsyncWebServerRequest* request) {
        handleWiFiSave(request);
    });
    
    server->on("/api/wifi/reset", HTTP_POST, [this](AsyncWebServerRequest* request) {
        handleWiFiReset(request);
    });
    
    server->on("/api/restart", HTTP_POST, [this](AsyncWebServerRequest* request) {
        request->send(200, "application/json", "{\"success\":true}");
        delay(200);
        
        // Clean WiFi shutdown
        Serial.println("Restart requested via API");
        if (server) {
            server->end();
        }
        WiFi.disconnect(true);
        WiFi.softAPdisconnect(true);
        WiFi.mode(WIFI_OFF);
        
        Serial.println("Restarting...");
        Serial.flush();
        delay(1000);
        
        ESP.restart();
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

void WebInterface::handleWiFiConfig(AsyncWebServerRequest* request) {
    String html = generateWiFiConfigPage();
    request->send(200, "text/html", html);
}

void WebInterface::handleWiFiScan(AsyncWebServerRequest* request) {
    JsonDocument doc;
    JsonArray networks = doc["networks"].to<JsonArray>();
    
    int n = WiFi.scanNetworks();
    for (int i = 0; i < n && i < 20; ++i) {
        JsonObject net = networks.add<JsonObject>();
        net["ssid"] = WiFi.SSID(i);
        net["rssi"] = WiFi.RSSI(i);
        net["secure"] = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
    }
    
    String output;
    serializeJson(doc, output);
    request->send(200, "application/json", output);
}

void WebInterface::handleWiFiSave(AsyncWebServerRequest* request) {
    Serial.println("=== WiFi Save Request ===");
    Serial.print("Params count: ");
    Serial.println(request->params());
    
    // Debug: print all parameters (except password for security)
    for(int i = 0; i < request->params(); i++) {
        const AsyncWebParameter* p = request->getParam(i);
        Serial.print("Param ");
        Serial.print(i);
        Serial.print(": ");
        Serial.print(p->name());
        Serial.print(" = ");
        
        // Don't print password values to serial
        if (String(p->name()) == "password") {
            Serial.println("***");
        } else {
            Serial.println(p->value());
        }
    }
    
    if (request->hasParam("ssid", true) && request->hasParam("password", true)) {
        String ssid = request->getParam("ssid", true)->value();
        String password = request->getParam("password", true)->value();
        bool useStation = false;
        
        // Check if enable parameter is explicitly "true"
        if (request->hasParam("enable", true)) {
            String enableValue = request->getParam("enable", true)->value();
            useStation = (enableValue == "true" || enableValue == "1" || enableValue == "on");
        }
        
        Serial.println("WiFi config save request:");
        Serial.print("  SSID: "); Serial.println(ssid);
        Serial.print("  Password length: "); Serial.println(password.length());
        Serial.print("  Enable station mode: "); Serial.println(useStation ? "YES" : "NO");
        
        // Save configuration
        strncpy(config.staSSID, ssid.c_str(), sizeof(config.staSSID) - 1);
        config.staSSID[sizeof(config.staSSID) - 1] = '\0';
        strncpy(config.staPassword, password.c_str(), sizeof(config.staPassword) - 1);
        config.staPassword[sizeof(config.staPassword) - 1] = '\0';
        config.useStationMode = useStation;
        config.isConfigured = true;
        saveConfig();
        
        Serial.println("Configuration saved to flash");
        
        request->send(200, "application/json", 
            "{\"success\":true,\"message\":\"Configuration saved successfully!\"}");
    } else {
        Serial.println("ERROR: Missing SSID or password parameter");
        request->send(400, "application/json", 
            "{\"success\":false,\"error\":\"Missing SSID or password\"}");
    }
}

void WebInterface::handleWiFiReset(AsyncWebServerRequest* request) {
    Serial.println("WiFi config reset requested");
    resetConfig();
    request->send(200, "application/json", 
        "{\"success\":true,\"message\":\"WiFi config reset. Restarting...\"}");
    
    // Ensure response is sent
    delay(200);
    
    // Clean shutdown of WiFi before restart
    Serial.println("Shutting down WiFi...");
    if (server) {
        server->end();
    }
    WiFi.disconnect(true);
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);
    
    Serial.println("Restarting in 1 second...");
    Serial.flush();
    delay(1000);
    
    ESP.restart();
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
                <p><strong>WiFi Mode:</strong> )rawliteral" + String(isAPMode ? "Access Point" : "Station") + R"rawliteral(</p>
                <p><strong>IP Address:</strong> )rawliteral" + ipAddress.toString() + R"rawliteral(</p>
            </div>
            <div class="controls" style="margin-top: 15px;">
                <button class="btn" onclick="location.href='/wifi'">WiFi Settings</button>
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
    JsonDocument doc;
    
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
    
    JsonDocument doc;
    
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
    JsonDocument doc;
    
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

String WebInterface::generateWiFiConfigPage() {
    String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>WiFi Configuration</title>
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body { font-family: Arial, sans-serif; background: #1a1a1a; color: #fff; padding: 20px; }
        .container { max-width: 800px; margin: 0 auto; }
        h1 { color: #00ff88; margin-bottom: 10px; }
        .back-link { color: #00ff88; text-decoration: none; margin-bottom: 20px; display: inline-block; }
        .back-link:hover { text-decoration: underline; }
        .card { background: #2a2a2a; border-radius: 8px; padding: 20px; margin-bottom: 20px; }
        .form-group { margin-bottom: 15px; }
        label { display: block; margin-bottom: 5px; color: #aaa; }
        input, select { width: 100%; padding: 10px; background: #333; border: 1px solid #555; border-radius: 4px; color: #fff; font-size: 1em; }
        input:focus, select:focus { outline: none; border-color: #00ff88; }
        .btn { padding: 12px 24px; background: #00ff88; color: #000; border: none; border-radius: 4px; cursor: pointer; font-size: 1em; font-weight: bold; }
        .btn:hover { background: #00cc6a; }
        .btn-secondary { background: #555; color: #fff; }
        .btn-secondary:hover { background: #666; }
        .btn-danger { background: #ff4444; color: #fff; }
        .btn-danger:hover { background: #cc0000; }
        .info { color: #aaa; line-height: 1.6; margin-bottom: 15px; }
        .network-list { list-style: none; }
        .network-item { padding: 10px; background: #333; margin-bottom: 5px; border-radius: 4px; cursor: pointer; display: flex; justify-content: space-between; }
        .network-item:hover { background: #444; }
        .network-signal { color: #00ff88; }
        .checkbox-group { display: flex; align-items: flex-start; gap: 10px; flex-wrap: wrap; }
        .checkbox-group input[type="checkbox"] { margin-top: 3px; flex-shrink: 0; }
        .checkbox-group label { flex: 1; min-width: 200px; }
        .message { padding: 10px; border-radius: 4px; margin-bottom: 15px; }
        .message.success { background: #00ff8833; border: 1px solid #00ff88; }
        .message.error { background: #ff444433; border: 1px solid #ff4444; }
        .button-group { display: flex; gap: 10px; flex-wrap: wrap; }
        @media (max-width: 600px) {
            .checkbox-group { flex-direction: column; align-items: flex-start; }
            .checkbox-group input[type="checkbox"] { margin-top: 0; }
        }
    </style>
</head>
<body>
    <div class="container">
        <a href="/" class="back-link">← Back to Dashboard</a>
        <h1>WiFi Configuration</h1>
        
        <div class="card">
            <h2>Current Status</h2>
            <div class="info">
                <p><strong>Mode:</strong> )rawliteral" + String(isAPMode ? "Access Point" : "Station") + R"rawliteral(</p>
                <p><strong>IP Address:</strong> )rawliteral" + ipAddress.toString() + R"rawliteral(</p>
                <p><strong>SSID:</strong> )rawliteral" + String(isAPMode ? WIFI_SSID : config.staSSID) + R"rawliteral(</p>
            </div>
        </div>
        
        <div class="card">
            <h2>Available Networks</h2>
            <button class="btn btn-secondary" onclick="scanNetworks()">Scan for Networks</button>
            <div id="networks" style="margin-top: 15px;"></div>
        </div>
        
        <div class="card">
            <h2>Configure WiFi</h2>
            <div id="message"></div>
            <form id="wifiForm">
                <div class="form-group">
                    <label for="ssid">Network SSID</label>
                    <input type="text" id="ssid" name="ssid" value=")rawliteral" + String(config.staSSID) + R"rawliteral(" required>
                </div>
                <div class="form-group">
                    <label for="password">Password</label>
                    <input type="password" id="password" name="password" placeholder="Enter network password">
                </div>
                <div class="form-group checkbox-group">
                    <input type="checkbox" id="enable" name="enable" )rawliteral" + String(config.useStationMode ? "checked" : "") + R"rawliteral(>
                    <label for="enable">Enable Station Mode (connect to this network on boot)</label>
                </div>
                <div class="info">
                    <strong>Note:</strong> Configuration takes effect after restart. If station mode fails, device will fall back to AP mode at 192.168.4.1.
                </div>
                <div class="button-group">
                    <button type="submit" class="btn">Save Configuration</button>
                    <button type="button" class="btn btn-danger" onclick="resetConfig()">Reset to AP Mode</button>
                </div>
            </form>
        </div>
    </div>
    
    <script>
        function scanNetworks() {
            document.getElementById('networks').innerHTML = '<p>Scanning...</p>';
            fetch('/api/wifi/scan')
                .then(r => r.json())
                .then(data => {
                    let html = '<ul class="network-list">';
                    data.networks.forEach(net => {
                        let signal = net.rssi > -60 ? '███' : net.rssi > -75 ? '██░' : '█░░';
                        let secure = net.secure ? '🔒' : '🔓';
                        html += `<li class="network-item" onclick="selectNetwork('${net.ssid}')">
                            <span>${secure} ${net.ssid}</span>
                            <span class="network-signal">${signal} ${net.rssi}dBm</span>
                        </li>`;
                    });
                    html += '</ul>';
                    document.getElementById('networks').innerHTML = html;
                });
        }
        
        function selectNetwork(ssid) {
            document.getElementById('ssid').value = ssid;
            document.getElementById('password').focus();
        }
        
        document.getElementById('wifiForm').addEventListener('submit', function(e) {
            e.preventDefault();
            console.log('Form submission started');
            
            let ssid = document.getElementById('ssid').value;
            let password = document.getElementById('password').value;
            let enable = document.getElementById('enable').checked ? 'true' : 'false';
            
            console.log('SSID:', ssid);
            console.log('Password length:', password.length);
            console.log('Enable:', enable);
            
            let formData = 'ssid=' + encodeURIComponent(ssid) + 
                          '&password=' + encodeURIComponent(password) + 
                          '&enable=' + enable;
            
            console.log('Sending request...');
            
            fetch('/api/wifi/save', {
                method: 'POST',
                headers: {'Content-Type': 'application/x-www-form-urlencoded'},
                body: formData
            })
            .then(r => {
                console.log('Response status:', r.status);
                return r.json();
            })
            .then(data => {
                console.log('Response data:', data);
                let msg = document.getElementById('message');
                if (data.success) {
                    msg.innerHTML = '<div class="message success">' + data.message + 
                        ' Restarting in 3 seconds...</div>';
                    setTimeout(function() {
                        console.log('Triggering restart...');
                        fetch('/api/restart', {method: 'POST'}).catch(() => {});
                        setTimeout(() => location.href = '/', 5000);
                    }, 3000);
                } else {
                    msg.innerHTML = '<div class="message error">' + data.error + '</div>';
                }
            })
            .catch(err => {
                console.error('Error:', err);
                let msg = document.getElementById('message');
                msg.innerHTML = '<div class="message error">Request failed: ' + err.message + '</div>';
            });
        });
        
        function resetConfig() {
            if (confirm('Reset WiFi configuration to AP mode only? Device will restart.')) {
                fetch('/api/wifi/reset', { method: 'POST' })
                    .then(r => r.json())
                    .then(data => {
                        let msg = document.getElementById('message');
                        msg.innerHTML = '<div class="message success">' + data.message + '</div>';
                        setTimeout(() => {
                            location.href = 'http://192.168.4.1/';
                        }, 3000);
                    })
                    .catch(err => {
                        console.log('Device restarting...');
                        setTimeout(() => {
                            location.href = 'http://192.168.4.1/';
                        }, 5000);
                    });
            }
        }
    </script>
</body>
</html>
)rawliteral";
    return html;
}

#endif // ENABLE_WEB_INTERFACE
