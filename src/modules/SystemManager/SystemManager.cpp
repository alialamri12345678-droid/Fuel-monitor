#include "SystemManager.h"
#include "../../config/Config.h"
#include "../../utils/Logger.h"
#include "../../utils/ErrorHandler.h"
#include "../../utils/DataTypes.h"

#include <ArduinoJson.h>
#include <Preferences.h>
#include <time.h>
#include "mbedtls/md.h"

static const char* TAG = "System";

// Static instance for MQTT callback routing
SystemManager* SystemManager::_instance = nullptr;

// Static error array for ErrorHandler
bool ErrorHandler::_errors[ErrorHandler::MAX_ERRORS] = {false};

// ============================================================
//  Constructor
// ============================================================

SystemManager::SystemManager()
    : _delivery(),
      _wifi(),
      _mqtt(),
      _modbus(Serial2, RS485_DE_RE_PIN),
      _flowModbus(_modbus),
      _lastModbusPoll(0),
      _initialized(false),
      _mqttSubscribed(false),
      _sessionTotalLiters(0.0f),
      _sequenceNumber(1),
      _lastSensorRead(0),
      _lastMqttPublish(0),
      _lastStatusPublish(0)
{
    _instance = this;
}

// ============================================================
//  Initialization
// ============================================================

bool SystemManager::begin()
{
    // Logger must be initialized first
    Logger::begin(SERIAL_BAUD_RATE, Logger::Level::INFO);

    LOG_INFO(TAG, "====================================");
    LOG_INFO(TAG, "  Diesel Delivery Monitor Starting");
    LOG_INFO(TAG, "====================================");

    if (!initializeModules())
    {
        LOG_ERROR(TAG, "Module initialization failed");
    }

    // Initialize hardware watchdog (10 second timeout, auto-reboot on hang)
    esp_task_wdt_init(10, true);
    esp_task_wdt_add(NULL);
    LOG_INFO(TAG, "Watchdog timer initialized (10s)");

    connectNetwork();
    connectMQTT();
    subscribeTopics();

    // Load persisted state from NVS
    loadSessionTotal();
    loadSequenceNumber();

    // Configure NTP for unix timestamps
    configTzTime("UTC0", "pool.ntp.org");

    _initialized = true;

    LOG_INFO(TAG, "====================================");
    LOG_INFO(TAG, "  System Ready");
    LOG_INFO(TAG, "====================================");
    Logger::printHeap();

    return true;
}

bool SystemManager::initializeModules()
{
    bool allOk = true;

    // 1. Flow Meter (Modbus RTU)
    LOG_INFO(TAG, "Initializing Modbus Flow Meter...");
    _modbus.begin(MODBUS_BAUD_RATE);
    _flowModbus.begin(MODBUS_SLAVE_ID);

    // 2. Delivery Manager
    LOG_INFO(TAG, "Initializing Delivery Manager...");
    _delivery.begin();

    // 3. MQTT (TLS)
    LOG_INFO(TAG, "Initializing MQTT (TLS)...");
    _mqtt.begin(MQTT_BROKER, MQTT_PORT,
                MQTT_USERNAME, MQTT_PASSWORD,
                MQTT_DEVICE_ID);
    _mqtt.setCallback(mqttCallbackStatic);

    return allOk;
}

void SystemManager::connectNetwork()
{
    LOG_INFO(TAG, "Initiating WiFi connection...");
    _wifi.begin(WIFI_SSID, WIFI_PASSWORD);
}

void SystemManager::connectMQTT()
{
    if (!_wifi.isConnected())
    {
        return;
    }

    LOG_INFO(TAG, "Connecting MQTT...");
    _mqtt.connect();
}

void SystemManager::subscribeTopics()
{
    if (!_mqtt.isConnected())
    {
        return;
    }

    LOG_INFO(TAG, "Subscribing to command topics...");

    char topic[MQTTManager::MAX_TOPIC_LEN];
    _mqtt.getCommandTopic(topic, sizeof(topic));
    _mqtt.subscribe(topic);

    _mqttSubscribed = true;
}

// ============================================================
//  Main Loop — Fully Non-blocking
// ============================================================

void SystemManager::update()
{
    if (!_initialized) return;

    // Feed the watchdog — prevents auto-reboot on healthy operation
    esp_task_wdt_reset();

    // --------------------------------------------------------
    //  1. WiFi monitoring (every cycle)
    // --------------------------------------------------------
    _wifi.update();

    // --------------------------------------------------------
    //  2. MQTT monitoring (every cycle)
    // --------------------------------------------------------
    if (_wifi.isConnected())
    {
        _mqtt.update();

        // Re-subscribe after reconnect
        if (_mqtt.isConnected() && !_mqttSubscribed)
        {
            subscribeTopics();
        }

        if (!_mqtt.isConnected())
        {
            _mqttSubscribed = false;
        }
    }

    // --------------------------------------------------------
    //  3. Sensor reading + Delivery detection (every 100ms)
    // --------------------------------------------------------
    if (timerExpired(_lastSensorRead, SENSOR_READ_INTERVAL_MS))
    {
        readSensors();
    }

    // --------------------------------------------------------
    //  4. MQTT telemetry publish (every 1 second)
    // --------------------------------------------------------
    if (timerExpired(_lastMqttPublish, MQTT_PUBLISH_INTERVAL_MS))
    {
        publishLiveData();
    }

    // --------------------------------------------------------
    //  5. Status publish (every 30 seconds)
    // --------------------------------------------------------
    if (timerExpired(_lastStatusPublish, STATUS_PUBLISH_INTERVAL_MS))
    {
        publishStatus();
    }

    // --------------------------------------------------------
    //  6. Delivery completion handling
    // --------------------------------------------------------
    handleDeliveryCompletion();
}

// ============================================================
//  Scheduling Helper
// ============================================================

bool SystemManager::timerExpired(unsigned long& lastTime,
                                  unsigned long interval)
{
    unsigned long now = millis();

    if (now - lastTime >= interval)
    {
        lastTime = now;
        return true;
    }

    return false;
}

// ============================================================
//  Data Processing
// ============================================================

void SystemManager::readSensors()
{
    // Modbus polling at its own interval (1 second)
    if (timerExpired(_lastModbusPoll, MODBUS_POLL_INTERVAL_MS))
    {
        _flowModbus.update();
    }

    // Feed delivery manager with Modbus flow rate (L/min)
    _delivery.update(_flowModbus.getFlowLPM());
}

void SystemManager::publishLiveData()
{
    const auto& md = _flowModbus.getData();
    LOG_INFO(TAG, "Live: %.1f Hz | %.2f L/min | %.1f°C | "
                  "%.2f m/s | Cum: %.3f m³ | State: %s",
             md.frequency,
             _flowModbus.getFlowLPM(),
             md.temperature,
             md.velocity,
             md.totalFlow,
             deliveryStateToString(_delivery.getState()));

    if (!_mqtt.isConnected()) return;

    char buffer[256];
    if (serializeTelemetry(buffer, sizeof(buffer)))
    {
        char topic[MQTTManager::MAX_TOPIC_LEN];
        _mqtt.getTelemetryTopic(topic, sizeof(topic));
        _mqtt.publish(topic, buffer);
    }
}

void SystemManager::publishStatus()
{
    if (!_mqtt.isConnected()) return;

    char buffer[512];
    if (serializeStatus(buffer, sizeof(buffer)))
    {
        char topic[MQTTManager::MAX_TOPIC_LEN];
        _mqtt.getTelemetryTopic(topic, sizeof(topic));
        _mqtt.publish(topic, buffer);
    }
}

void SystemManager::publishDeliveryRecord(const DeliveryRecord& record)
{
    // Delivery data is included in the telemetry stream.
    // Publish a one-time telemetry message with the delivery totals.
    if (!_mqtt.isConnected()) return;

    char buffer[256];
    if (serializeTelemetry(buffer, sizeof(buffer)))
    {
        char topic[MQTTManager::MAX_TOPIC_LEN];
        _mqtt.getTelemetryTopic(topic, sizeof(topic));
        _mqtt.publish(topic, buffer);
    }
}

void SystemManager::handleDeliveryCompletion()
{
    if (_delivery.hasNewDelivery())
    {
        const DeliveryRecord& record = _delivery.getLastDelivery();

        // Update session total and persist to NVS
        _sessionTotalLiters += record.totalLiters;
        saveSessionTotal();

        // Publish delivery via telemetry
        publishDeliveryRecord(record);
    }
}



// ============================================================
//  JSON Serialization
// ============================================================

bool SystemManager::serializeTelemetry(char* buffer, size_t len)
{
    StaticJsonDocument<256> doc;

    float totalVol = _flowModbus.getCumulativeLiters();
    doc["total_volume"] = totalVol;

    size_t written = serializeJson(doc, buffer, len);
    return (written > 0 && written < len);
}

bool SystemManager::serializeStatus(char* buffer, size_t len)
{
    StaticJsonDocument<512> doc;

    doc["device_id"]    = MQTT_DEVICE_ID;
    doc["online"]       = true;
    doc["uptime_s"]     = millis() / 1000;

    // WiFi status
    JsonObject wifi = doc.createNestedObject("wifi");
    wifi["connected"]   = _wifi.isConnected();
    wifi["rssi"]        = _wifi.getRSSI();
    wifi["quality"]     = _wifi.getSignalQuality();
    wifi["ip"]          = _wifi.getIPAddress().toString();

    // Modbus status
    doc["modbus_online"] = _flowModbus.isOnline();
    doc["free_heap"]     = ESP.getFreeHeap();
    doc["deliveries"]    = _delivery.getDeliveryCount();
    doc["seq"]           = _sequenceNumber;

    // Active errors
    char errStr[128];
    ErrorHandler::getActiveErrorsString(errStr, sizeof(errStr));
    doc["errors"] = errStr;

    size_t written = serializeJson(doc, buffer, len);
    return (written > 0 && written < len);
}

// ============================================================
//  MQTT Callback
// ============================================================

void SystemManager::mqttCallbackStatic(
    char* topic,
    uint8_t* payload,
    unsigned int length)
{
    if (_instance != nullptr)
    {
        _instance->handleMQTTMessage(topic, payload, length);
    }
}

void SystemManager::handleMQTTMessage(
    const char* topic,
    const uint8_t* payload,
    unsigned int length)
{
    if (length == 0 || length > 1024)
    {
        LOG_WARNING(TAG, "Invalid payload length: %u", length);
        return;
    }

    // Copy payload to null-terminated string
    char message[256];
    size_t copyLen = (length < sizeof(message) - 1)
                     ? length
                     : sizeof(message) - 1;
    memcpy(message, payload, copyLen);
    message[copyLen] = '\0';

    LOG_INFO(TAG, "MQTT cmd: %s -> %s", topic, message);

    // Check if this is a command topic
    char cmdTopic[MQTTManager::MAX_TOPIC_LEN];
    _mqtt.getCommandTopic(cmdTopic, sizeof(cmdTopic));

    if (strcmp(topic, cmdTopic) == 0)
    {
        StaticJsonDocument<256> doc;
        DeserializationError error = deserializeJson(doc, message);

        if (error)
        {
            LOG_WARNING(TAG, "JSON parse error: %s", error.c_str());
            return;
        }

        // Handle commands
        const char* cmd = doc["command"] | "";
        const char* clearCmd = doc["clear_command"] | "";

        if (strcmp(cmd, "CLEAR") == 0 || strcmp(clearCmd, "CLEAR") == 0)
        {
            _flowModbus.clearCumulative();
            LOG_INFO(TAG, "Cumulative flow cleared via MQTT command");
        }
        else if (strcmp(cmd, "status") == 0)
        {
            publishStatus();
        }
        else
        {
            LOG_WARNING(TAG, "Unknown command: %s", cmd);
        }

        return;
    }

    LOG_DEBUG(TAG, "Unhandled topic: %s", topic);
}

// ============================================================
//  NVS Persistence
// ============================================================

static const char* NVS_NAMESPACE = "diesel";
static const char* NVS_KEY_TOTAL = "total_liters";
static const char* NVS_KEY_SEQ   = "seq_num";

void SystemManager::loadSessionTotal()
{
    Preferences prefs;
    prefs.begin(NVS_NAMESPACE, true);
    _sessionTotalLiters = prefs.getFloat(NVS_KEY_TOTAL, 0.0f);
    prefs.end();

    LOG_INFO(TAG, "Loaded session total from NVS: %.2f liters",
             _sessionTotalLiters);
}

void SystemManager::saveSessionTotal()
{
    Preferences prefs;
    prefs.begin(NVS_NAMESPACE, false);
    prefs.putFloat(NVS_KEY_TOTAL, _sessionTotalLiters);
    prefs.end();

    LOG_DEBUG(TAG, "Saved session total to NVS: %.2f liters",
              _sessionTotalLiters);
}

void SystemManager::loadSequenceNumber()
{
    Preferences prefs;
    prefs.begin(NVS_NAMESPACE, true);
    _sequenceNumber = prefs.getULong(NVS_KEY_SEQ, 1);
    prefs.end();

    LOG_INFO(TAG, "Loaded sequence number from NVS: %lu",
             _sequenceNumber);
}

void SystemManager::saveSequenceNumber()
{
    Preferences prefs;
    prefs.begin(NVS_NAMESPACE, false);
    prefs.putULong(NVS_KEY_SEQ, _sequenceNumber);
    prefs.end();

    LOG_DEBUG(TAG, "Saved sequence number to NVS: %lu",
              _sequenceNumber);
}
