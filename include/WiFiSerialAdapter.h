/**
 * @file WiFiSerialAdapter.h
 * @brief WiFi TCP/IP socket implementation of ISerialInterface
 * 
 * Provides a drop-in replacement for hardware serial that communicates
 * over a WiFi TCP socket. A remote client (e.g. TunerStudio) connects
 * to the configured TCP port and sends/receives data as if it were a
 * serial connection.
 * 
 * Features:
 * - Listens for a single client on a configurable TCP port
 * - Transparent reconnection when client disconnects
 * - Ring buffer for efficient read operations
 * - Compatible with ESP32 and ESP8266
 * 
 * Requires ENABLE_WIFI to be defined.
 */

#ifndef WIFI_SERIAL_ADAPTER_H
#define WIFI_SERIAL_ADAPTER_H

#ifdef ENABLE_WIFI

#include "ISerialInterface.h"
#include "Config.h"

#ifdef ESP32
  #include <WiFi.h>
#elif defined(ESP8266)
  #include <ESP8266WiFi.h>
#endif

/**
 * @class WiFiSerialAdapter
 * @brief ISerialInterface implementation using a WiFi TCP/IP socket
 * 
 * Usage:
 * @code
 * WiFiSerialAdapter* wifiSerial = new WiFiSerialAdapter(WIFI_SERIAL_PORT);
 * wifiSerial->begin(0);  // baudRate is ignored for TCP
 * // Use exactly like hardware serial via ISerialInterface*
 * @endcode
 * 
 * The adapter creates a WiFiServer that accepts one client at a time.
 * All read/write calls are forwarded to the connected WiFiClient.
 * If no client is connected, reads return 0/empty and writes are
 * silently dropped.
 */
class WiFiSerialAdapter : public ISerialInterface {
public:
    /**
     * @brief Constructor
     * @param port TCP port to listen on (default from WIFI_SERIAL_PORT)
     */
    explicit WiFiSerialAdapter(uint16_t port = WIFI_SERIAL_PORT)
        : server(port), port(port), clientConnected(false),
          started(false), beginRequested(false) {}

    ~WiFiSerialAdapter() {
        if (client) {
            client.stop();
        }
        if (started) {
            server.stop();
        }
    }

    /**
     * @brief Request the TCP server to start
     * @param baudRate Ignored for TCP — kept for interface compatibility
     * 
     * The actual server.begin() is deferred until WiFi is connected,
     * because calling it before the LwIP TCP/IP stack is initialised
     * causes an "Invalid mbox" assert crash on ESP-IDF.
     */
    void begin(uint32_t baudRate) override {
        (void)baudRate;  // Not applicable to TCP sockets
        beginRequested = true;
        // Try to start immediately if WiFi is already up
        tryStartServer();
    }

    /**
     * @brief Check if a remote client is connected
     * @return true if a client is connected and the server has started
     */
    bool isReady() override {
        checkForClient();
        return started && clientConnected;
    }

    /**
     * @brief Get number of bytes available to read from client
     * @return Number of bytes in receive buffer, 0 if no client
     */
    int available() override {
        checkForClient();
        if (clientConnected && client.connected()) {
            return client.available();
        }
        return 0;
    }

    /**
     * @brief Read a single byte from the TCP socket
     * @return Byte read, or -1 if none available / no client
     */
    int read() override {
        checkForClient();
        if (clientConnected && client.connected()) {
            return client.read();
        }
        return -1;
    }

    /**
     * @brief Read multiple bytes from the TCP socket
     * @param buffer Destination buffer
     * @param length Maximum bytes to read
     * @return Number of bytes actually read
     */
    size_t readBytes(uint8_t* buffer, size_t length) override {
        checkForClient();
        if (clientConnected && client.connected() && client.available()) {
            return client.readBytes(buffer, length);
        }
        return 0;
    }

    /**
     * @brief Write a single byte to the TCP socket
     * @param byte Byte to write
     * @return 1 if written, 0 if no client connected
     */
    size_t write(uint8_t byte) override {
        checkForClient();
        if (clientConnected && client.connected()) {
            return client.write(byte);
        }
        return 0;
    }

    /**
     * @brief Write multiple bytes to the TCP socket
     * @param buffer Source buffer
     * @param length Number of bytes to write
     * @return Number of bytes actually written
     */
    size_t write(const uint8_t* buffer, size_t length) override {
        checkForClient();
        if (clientConnected && client.connected()) {
            return client.write(buffer, length);
        }
        return 0;
    }

    /**
     * @brief Flush the TCP output buffer
     */
    void flush() override {
        if (clientConnected && client.connected()) {
            client.flush();
        }
    }

    /**
     * @brief Clear the TCP input buffer
     */
    void clear() override {
        if (clientConnected && client.connected()) {
            while (client.available() > 0) {
                client.read();
            }
        }
    }

    /**
     * @brief Get the TCP port this adapter is listening on
     * @return TCP port number
     */
    uint16_t getPort() const { return port; }

    /**
     * @brief Check if a remote client is currently connected
     * @return true if connected
     */
    bool hasClient() const { return clientConnected; }

private:
    WiFiServer server;
    WiFiClient client;
    uint16_t   port;
    bool       clientConnected;
    bool       started;
    bool       beginRequested;

    /**
     * @brief Check if WiFi is connected (AP or STA mode)
     * @return true if the WiFi stack is up and ready
     */
    bool isWiFiReady() {
        #ifdef ESP32
            return WiFi.getMode() != WIFI_MODE_NULL &&
                   (WiFi.status() == WL_CONNECTED ||
                    WiFi.getMode() == WIFI_MODE_AP ||
                    WiFi.getMode() == WIFI_MODE_APSTA);
        #elif defined(ESP8266)
            return WiFi.getMode() != WIFI_OFF &&
                   (WiFi.status() == WL_CONNECTED ||
                    WiFi.getMode() == WIFI_AP ||
                    WiFi.getMode() == WIFI_AP_STA);
        #else
            return false;
        #endif
    }

    /**
     * @brief Actually start the TCP server if WiFi is ready
     */
    void tryStartServer() {
        if (started || !beginRequested) return;
        if (!isWiFiReady()) return;

        server.begin();
        server.setNoDelay(true);
        started = true;
        Serial.print("[WiFiSerial] Listening on port ");
        Serial.println(port);
    }

    /**
     * @brief Poll for new client connections and detect disconnects
     * 
     * Called internally before every read/write to keep the connection
     * state up-to-date. Accepts at most one client at a time.
     * Also handles deferred server start once WiFi becomes available.
     */
    void checkForClient() {
        // Handle deferred start
        if (!started) {
            tryStartServer();
            if (!started) return;
        }

        // Detect disconnection of current client
        if (clientConnected && !client.connected()) {
            client.stop();
            clientConnected = false;
            Serial.println("[WiFiSerial] Client disconnected");
        }

        // Accept a new client if none is connected
        if (!clientConnected) {
            WiFiClient newClient = server.available();
            if (newClient) {
                client = newClient;
                client.setNoDelay(true);
                clientConnected = true;
                Serial.print("[WiFiSerial] Client connected from ");
                Serial.println(client.remoteIP());
            }
        }
    }
};

#endif // ENABLE_WIFI
#endif // WIFI_SERIAL_ADAPTER_H
