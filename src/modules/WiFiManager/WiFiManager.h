#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>

/**
 * ============================================================
 * WiFiManager
 * ------------------------------------------------------------
 * Professional non-blocking WiFi Manager for ESP32.
 *
 * Features:
 *  - Automatic connection and reconnection
 *  - Non-blocking update loop (no delay())
 *  - Connection timeout with backoff
 *  - WiFi signal monitoring (RSSI / quality)
 *  - Configurable hostname
 *
 * Adapted from the Generator Monitoring project.
 * ============================================================
 */

class WiFiManager
{
public:

    enum class State : uint8_t
    {
        DISCONNECTED,
        CONNECTING,
        CONNECTED,
        CONNECTION_FAILED,
        RECONNECTING
    };

    WiFiManager();

    /**
     * @brief Initialize and start WiFi connection.
     */
    void begin(const String& ssid,
               const String& password);

    /**
     * @brief Non-blocking update. Call in loop().
     */
    void update();

    /**
     * @brief Disconnect from WiFi.
     */
    void disconnect();

    /**
     * @brief Returns true if connected to WiFi.
     */
    bool isConnected() const;

    /**
     * @brief Current connection state.
     */
    State getState() const;

    /**
     * @brief Current IP address.
     */
    IPAddress getIPAddress() const;

    /**
     * @brief RSSI in dBm.
     */
    int getRSSI() const;

    /**
     * @brief Signal quality (0-100%).
     */
    uint8_t getSignalQuality() const;

    /**
     * @brief Connected SSID.
     */
    const String& getSSID() const;

    /**
     * @brief MAC Address.
     */
    String getMACAddress() const;

    /**
     * @brief WiFi uptime in milliseconds.
     */
    unsigned long getConnectedTime() const;

    /**
     * @brief Number of reconnect attempts.
     */
    uint32_t getReconnectCount() const;

    /**
     * @brief Set hostname.
     */
    void setHostname(const char* hostname);

private:

    String _ssid;
    String _password;

    const char* _hostname;

    State _state;

    unsigned long _connectStartTime;
    unsigned long _lastReconnectTime;
    unsigned long _connectedSince;

    uint32_t _reconnectCount;

    static constexpr unsigned long CONNECTION_TIMEOUT  = 15000;
    static constexpr unsigned long RECONNECT_INTERVAL  = 5000;

    void startConnection();
    void checkConnection();

    static void wifiEvent(WiFiEvent_t event);
};

#endif // WIFI_MANAGER_H
