/**
 * @file WebInterface.h
 * @brief Web-based configuration and monitoring interface
 * 
 * ESP32/ESP8266 only feature providing:
 * - Real-time engine parameter display
 * - Simulation mode control
 * - Configuration via web UI
 * - REST API for external integration
 */

#ifndef WEB_INTERFACE_H
#define WEB_INTERFACE_H

#ifdef ENABLE_WEB_INTERFACE

#include "EngineSimulator.h"
#include "SpeeduinoProtocol.h"
#include "Config.h"

#ifdef ESP32
  #include <WiFi.h>
  #include <ESPAsyncWebServer.h>
  #include <ESPmDNS.h>
#elif defined(ESP8266)
  #include <ESP8266WiFi.h>
  #include <ESPAsyncWebServer.h>
  #include <ESPAsyncTCP.h>
  #include <ESP8266mDNS.h>
#endif

#include <ArduinoJson.h>

/**
 * @class WebInterface
 * @brief Web server for simulator configuration and monitoring
 */
class WebInterface {
private:
    AsyncWebServer* server;
    EngineSimulator* simulator;
    SpeeduinoProtocol* protocol;
    bool wifiConnected;
    IPAddress ipAddress;
    
public:
    /**
     * @brief Constructor
     * @param simulator Engine simulator reference
     * @param protocol Protocol handler reference
     */
    WebInterface(EngineSimulator* simulator, SpeeduinoProtocol* protocol);
    
    /**
     * @brief Destructor
     */
    ~WebInterface();
    
    /**
     * @brief Initialize WiFi and web server
     * @param apMode true for AP mode, false for station mode
     * @return true if successful
     */
    bool begin(bool apMode = true);
    
    /**
     * @brief Check WiFi connection status
     * @return true if connected
     */
    bool isConnected() const { return wifiConnected; }
    
    /**
     * @brief Get IP address
     * @return Current IP address
     */
    IPAddress getIP() const { return ipAddress; }
    
    /**
     * @brief Update WiFi and handle reconnection
     */
    void update();
    
private:
    // WiFi management
    void setupWiFiAP();
    void setupWiFiStation();
    
    // Route handlers
    void setupRoutes();
    void handleRoot(AsyncWebServerRequest* request);
    void handleStatus(AsyncWebServerRequest* request);
    void handleSetMode(AsyncWebServerRequest* request);
    void handleRealtimeData(AsyncWebServerRequest* request);
    void handleStatistics(AsyncWebServerRequest* request);
    void handleNotFound(AsyncWebServerRequest* request);
    
    // HTML pages
    String generateHomePage();
    String generateDashboard();
    
    // JSON helpers
    String getStatusJSON();
    String getRealtimeJSON();
    String getStatisticsJSON();
};

#endif // ENABLE_WEB_INTERFACE
#endif // WEB_INTERFACE_H
