#ifndef WEB_DASHBOARD_H
#define WEB_DASHBOARD_H

#include <Arduino.h>
#include <ESPAsyncWebServer.h>

/**
 * ============================================================
 * WebDashboard
 * ------------------------------------------------------------
 * ESP32-hosted web dashboard served via AsyncWebServer.
 *
 * Features:
 *  - Serves HTML dashboard on port 80
 *  - WebSocket (/ws) for real-time data push
 *  - Receives commands from browser (reset total, etc.)
 *  - Non-blocking — runs on the async TCP stack
 *
 * Usage:
 *  1. Call begin() after WiFi is connected
 *  2. Call pushData(json) every second to update clients
 *  3. Set onCommand callback to handle reset commands
 * ============================================================
 */

// Command callback type
typedef void (*DashboardCommandCallback)(const char* command);

class WebDashboard
{
public:

    WebDashboard();

    /**
     * @brief Start the web server and WebSocket.
     * @param port HTTP port (default 80).
     */
    void begin(uint16_t port = 80);

    /**
     * @brief Push JSON data to all connected WebSocket clients.
     * @param jsonData  Serialized JSON string.
     */
    void pushData(const char* jsonData);

    /**
     * @brief Set callback for incoming dashboard commands.
     */
    void onCommand(DashboardCommandCallback cb);

    /**
     * @brief Clean up disconnected WebSocket clients.
     *        Call periodically (e.g. every 5 seconds).
     */
    void cleanupClients();

    /**
     * @brief Get number of connected WebSocket clients.
     */
    size_t getClientCount() const;

private:

    AsyncWebServer  _server;
    AsyncWebSocket  _ws;

    DashboardCommandCallback _commandCallback;

    // WebSocket event handler
    static void onWsEvent(
        AsyncWebSocket* server,
        AsyncWebSocketClient* client,
        AwsEventType type,
        void* arg,
        uint8_t* data,
        size_t len);

    // Static instance for callback routing
    static WebDashboard* _instance;
};

#endif // WEB_DASHBOARD_H
