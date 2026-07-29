#ifndef MQTT_MANAGER_H
#define MQTT_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>

/**
 * ============================================================
 * MQTTManager
 * ------------------------------------------------------------
 * Manages MQTT connection, publishing, and subscriptions
 * for the Diesel Delivery Monitoring System.
 *
 * Uses plain TCP (WiFiClient) for MQTT communication.
 *
 * Features:
 *  - Non-blocking connection and reconnection
 *  - Publish to diesel-specific topics
 *  - Subscribe to command topics
 *  - Callback routing for incoming messages
 *  - Topic building with device ID substitution
 *
 * Adapted from the Generator Monitoring project.
 * ============================================================
 */

class MQTTManager
{
public:

    MQTTManager();

    /**
     * @brief Initialize MQTT with broker credentials.
     * @return true on success.
     */
    bool begin(const char* host,
               uint16_t port,
               const char* username,
               const char* password,
               const char* deviceId);

    /**
     * @brief Non-blocking update. Maintains connection.
     *        Call in loop().
     */
    void update();

    /**
     * @brief Attempt to connect to broker.
     * @return true if connected.
     */
    bool connect();

    /**
     * @brief Disconnect from broker.
     */
    void disconnect();

    /**
     * @brief Check if connected to broker.
     */
    bool isConnected();

    /**
     * @brief Publish a message to a topic.
     * @param topic   MQTT topic string.
     * @param payload JSON payload string.
     * @param retained If true, message is retained by broker.
     * @return true if published successfully.
     */
    bool publish(const char* topic,
                 const char* payload,
                 bool retained = false);

    /**
     * @brief Subscribe to a topic.
     */
    bool subscribe(const char* topic);

    /**
     * @brief Set the MQTT message callback.
     */
    void setCallback(MQTT_CALLBACK_SIGNATURE);

    /**
     * @brief Set PubSubClient buffer size.
     */
    void setBufferSize(uint16_t size);

    /**
     * @brief Get the underlying PubSubClient (for buffer processing).
     */
    PubSubClient& getClient();

    // ========================================================
    //  Topic Builders
    // ========================================================

    /**
     * @brief Build the live data topic.
     *        diesel/device/{device_id}/data
     */
    void getDataTopic(char* buffer, size_t len) const;

    /**
     * @brief Build the status topic.
     *        diesel/device/{device_id}/status
     */
    void getStatusTopic(char* buffer, size_t len) const;

    /**
     * @brief Build the delivery topic.
     *        diesel/device/{device_id}/delivery
     */
    void getDeliveryTopic(char* buffer, size_t len) const;

    /**
     * @brief Build the command topic (subscribe).
     *        diesel/device/{device_id}/command
     */
    void getCommandTopic(char* buffer, size_t len) const;

    /**
     * @brief Maximum topic length.
     */
    static constexpr size_t MAX_TOPIC_LEN = 80;

private:

    WiFiClient    _wifiClient;
    PubSubClient  _mqttClient;

    String _brokerHost;
    uint16_t _brokerPort;
    String _mqttUsername;
    String _mqttPassword;
    String _deviceId;
    String _clientId;

    unsigned long _lastReconnectAttempt;

    static constexpr unsigned long RECONNECT_INTERVAL  = 5000;
    static constexpr uint16_t     DEFAULT_BUFFER_SIZE  = 2048;

    /**
     * @brief Generate a unique client ID from ESP32 chip ID.
     */
    void generateClientId();
};

#endif // MQTT_MANAGER_H
