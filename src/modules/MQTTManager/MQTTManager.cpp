#include "MQTTManager.h"
#include "../../config/Config.h"
#include "../../config/Certs.h"
#include "../../utils/Logger.h"
#include "../../utils/ErrorHandler.h"

static const char* TAG = "MQTT";

MQTTManager::MQTTManager()
    : _mqttClient(_wifiClient),
      _brokerPort(8883),
      _lastReconnectAttempt(0)
{
}

bool MQTTManager::begin(const char* host,
                        uint16_t port,
                        const char* username,
                        const char* password,
                        const char* deviceId)
{
    _brokerHost   = host;
    _brokerPort   = port;
    _mqttUsername = username;
    _mqttPassword = password;
    _deviceId     = deviceId;

    // Set up mutual TLS (mTLS) with certificates
    _wifiClient.setCACert(EMQX_CA_CERT);
    _wifiClient.setCertificate(MQTTX_CLIENT_CERT);
    _wifiClient.setPrivateKey(MQTTX_CLIENT_KEY);

    _mqttClient.setServer(
        _brokerHost.c_str(),
        _brokerPort);

    _mqttClient.setBufferSize(DEFAULT_BUFFER_SIZE);

    generateClientId();

    LOG_INFO(TAG, "Configured: %s:%u (TLS)", _brokerHost.c_str(), _brokerPort);
    LOG_INFO(TAG, "Device ID: %s", _deviceId.c_str());
    LOG_INFO(TAG, "Client ID: %s", _clientId.c_str());

    return true;
}

void MQTTManager::update()
{
    if (_mqttClient.connected())
    {
        _mqttClient.loop();
        return;
    }

    unsigned long now = millis();

    if (now - _lastReconnectAttempt >= RECONNECT_INTERVAL)
    {
        _lastReconnectAttempt = now;

        LOG_INFO(TAG, "Attempting TLS connection...");

        connect();
    }
}

bool MQTTManager::connect()
{
    if (_mqttClient.connected())
    {
        return true;
    }

    // Build LWT topic: devices/{id}/status
    char lwtTopic[MAX_TOPIC_LEN];
    getStatusTopic(lwtTopic, sizeof(lwtTopic));

    // LWT payload matches Flutter app expectation
    const char* lwtPayload = "{\"status\": \"offline\"}";

    // Connect with LWT
    const char* user = _mqttUsername.length() > 0 ? _mqttUsername.c_str() : nullptr;
    const char* pass = _mqttPassword.length() > 0 ? _mqttPassword.c_str() : nullptr;

    bool connected = _mqttClient.connect(
        _clientId.c_str(),
        user,
        pass,
        lwtTopic,
        1,              // QoS 1
        true,           // retained
        lwtPayload);

    if (connected)
    {
        LOG_INFO(TAG, "Connected (TLS) as %s", _clientId.c_str());

        // Publish online status immediately (retained)
        publish(lwtTopic, "{\"status\": \"online\"}", true);

        ErrorHandler::clearError(SystemError::ERROR_MQTT_FAILURE);
    }
    else
    {
        LOG_WARNING(TAG, "Connection failed, rc=%d",
                    _mqttClient.state());
        ErrorHandler::setError(SystemError::ERROR_MQTT_FAILURE);
    }

    return connected;
}

void MQTTManager::disconnect()
{
    _mqttClient.disconnect();
    LOG_INFO(TAG, "Disconnected");
}

bool MQTTManager::isConnected()
{
    return _mqttClient.connected();
}

bool MQTTManager::publish(const char* topic,
                           const char* payload,
                           bool retained)
{
    if (!_mqttClient.connected())
    {
        return false;
    }

    bool success = _mqttClient.publish(
        topic, payload, retained);

    if (!success)
    {
        LOG_WARNING(TAG, "Publish failed: %s", topic);
    }

    return success;
}

bool MQTTManager::subscribe(const char* topic)
{
    if (!_mqttClient.connected())
    {
        return false;
    }

    bool success = _mqttClient.subscribe(topic);

    if (success)
    {
        LOG_INFO(TAG, "Subscribed: %s", topic);
    }
    else
    {
        LOG_WARNING(TAG, "Subscribe failed: %s", topic);
    }

    return success;
}

void MQTTManager::setCallback(MQTT_CALLBACK_SIGNATURE)
{
    _mqttClient.setCallback(callback);
}

void MQTTManager::setBufferSize(uint16_t size)
{
    _mqttClient.setBufferSize(size);
}

PubSubClient& MQTTManager::getClient()
{
    return _mqttClient;
}

// ============================================================
//  Topic Builders (Flutter-compatible)
// ============================================================

void MQTTManager::getTelemetryTopic(char* buffer, size_t len) const
{
    snprintf(buffer, len, "fuel_monitor_data");
}

void MQTTManager::getStatusTopic(char* buffer, size_t len) const
{
    snprintf(buffer, len, "%s/%s/status",
             MQTT_TOPIC_PREFIX, _deviceId.c_str());
}

void MQTTManager::getCommandTopic(char* buffer, size_t len) const
{
    snprintf(buffer, len, "fuel_monitor_command");
}

// ============================================================
//  Private
// ============================================================

void MQTTManager::generateClientId()
{
    _clientId = "fuel_monitor_firmware";
}
