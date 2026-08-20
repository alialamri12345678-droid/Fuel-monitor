#include "SystemManager.h"
#include "../../config/Config.h"
#include "../../utils/Logger.h"
#include "../../utils/ErrorHandler.h"
#include "../../utils/DataTypes.h"

#include <ArduinoJson.h>
#include <Preferences.h>
#include <time.h>

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
      _wakeupCause(ESP_SLEEP_WAKEUP_UNDEFINED),
      _hasPendingEvent(false),
      _eventPublished(false),
      _lastFlowDetectedMs(0),
      _flowDetectedSinceWakeup(false),
      _lastSensorRead(0),
      _lastSerialLog(0)
{
    _instance = this;
}

// ============================================================
//  Initialization
// ============================================================

bool SystemManager::begin(esp_sleep_wakeup_cause_t wakeupCause)
{
    _wakeupCause = wakeupCause;

    // Logger must be initialized first
    Logger::begin(SERIAL_BAUD_RATE, Logger::Level::INFO);

    LOG_INFO(TAG, "====================================");
    LOG_INFO(TAG, "  Diesel Delivery Monitor Starting");
    LOG_INFO(TAG, "====================================");

    // Log wake-up cause
    switch (_wakeupCause)
    {
        case ESP_SLEEP_WAKEUP_EXT0:
            LOG_INFO(TAG, "Wake-up cause: EXT0 (flow pulse on GPIO %d)", (int)WAKEUP_PIN);
            break;
        case ESP_SLEEP_WAKEUP_EXT1:
            LOG_INFO(TAG, "Wake-up cause: EXT1");
            break;
        case ESP_SLEEP_WAKEUP_TIMER:
            LOG_INFO(TAG, "Wake-up cause: TIMER");
            break;
        default:
            LOG_INFO(TAG, "Wake-up cause: POWER ON / RESET");
            break;
    }

    if (!initializeModules())
    {
        LOG_ERROR(TAG, "Module initialization failed");
    }

    // Initialize hardware watchdog (10 second timeout, auto-reboot on hang)
    esp_task_wdt_init(10, true);
    esp_task_wdt_add(NULL);
    LOG_INFO(TAG, "Watchdog timer initialized (10s)");

    // Configure ext0 deep sleep wake-up on flowmeter pulse pin
    // Wake on LOW level (NPN open-collector pulls to GND when active)
    esp_sleep_enable_ext0_wakeup(WAKEUP_PIN, 0);
    pinMode(WAKEUP_PIN, INPUT_PULLUP);  // Also use as digital input while awake
    LOG_INFO(TAG, "Deep sleep ext0 wake-up configured on GPIO %d (Active LOW)", (int)WAKEUP_PIN);

    // Initialize flow tracking
    _lastFlowDetectedMs = millis();
    _flowDetectedSinceWakeup = false;

    connectNetwork();
    connectMQTT();
    subscribeTopics();

    // Configure NTP for timestamps
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
//  Main Loop — Event-Based, Non-blocking
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
    //  4. Delivery completion handling
    // --------------------------------------------------------
    handleDeliveryCompletion();

    // --------------------------------------------------------
    //  5. Publish pending event (retry until success)
    // --------------------------------------------------------
    publishPendingEvent();

    // --------------------------------------------------------
    //  6. Deep sleep check (battery conservation)
    //     Only sleeps after pending event has been published
    // --------------------------------------------------------
    checkSleepCondition();
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

    // Detect flow from frequency pulse on GPIO 33
    // NPN open-collector: LOW = flow active, HIGH = no flow
    bool flowActive = (digitalRead(WAKEUP_PIN) == LOW);

    // Track flow activity for deep sleep and delivery detection
    if (flowActive)
    {
        _lastFlowDetectedMs = millis();
        _flowDetectedSinceWakeup = true;
    }

    // Feed delivery manager with a synthetic flow rate based on GPIO state
    // (Modbus registers don't provide instantaneous flow rate for this sensor)
    float syntheticFlowLPM = flowActive ? (DELIVERY_START_THRESHOLD_LPM + 1.0f) : 0.0f;
    _delivery.update(syntheticFlowLPM);

    // Periodic serial log (every 5 seconds)
    if (timerExpired(_lastSerialLog, 5000))
    {
        const auto& md = _flowModbus.getData();
        LOG_INFO(TAG, "Pulse: %s | Cum: %.3f %s | State: %s",
                 flowActive ? "ACTIVE" : "idle",
                 md.totalLiters,
                 _flowModbus.getUnitStr().c_str(),
                 deliveryStateToString(_delivery.getState()));
    }
}

void SystemManager::handleDeliveryCompletion()
{
    if (!_delivery.hasNewDelivery()) return;

    const DeliveryRecord& record = _delivery.getLastDelivery();

    LOG_INFO(TAG, "=== EVENT: Delivery Complete ===");
    LOG_INFO(TAG, "  Start:  %s", record.startTime);
    LOG_INFO(TAG, "  Stop:   %s", record.endTime);
    LOG_INFO(TAG, "  Volume: %.2f %s", record.totalLiters, _flowModbus.getUnitStr().c_str());

    // Read the actual cumulative volume from the sensor
    double sensorVolume = _flowModbus.getCumulativeLiters();
    LOG_INFO(TAG, "  Sensor cumulative: %.2f %s", sensorVolume, _flowModbus.getUnitStr().c_str());

    // Store the pending event (use sensor volume for accuracy)
    _pendingEvent = record;
    _pendingEvent.totalLiters = (float)sensorVolume;
    _hasPendingEvent = true;
    _eventPublished = false;

    // Save to NVS event history (circular buffer of 5)
    saveEventToHistory(_pendingEvent, _flowModbus.getUnitStr());

    // Clear the sensor's cumulative counter
    _flowModbus.clearCumulative();
    LOG_INFO(TAG, "Sensor cumulative cleared");
}

void SystemManager::publishPendingEvent()
{
    if (!_hasPendingEvent || _eventPublished) return;

    if (!_mqtt.isConnected())
    {
        // Keep retrying — WiFi/MQTT reconnect happens in the main loop
        return;
    }

    char buffer[256];
    if (serializeEvent(_pendingEvent, buffer, sizeof(buffer)))
    {
        char topic[MQTTManager::MAX_TOPIC_LEN];
        _mqtt.getTelemetryTopic(topic, sizeof(topic));

        if (_mqtt.publish(topic, buffer))
        {
            LOG_INFO(TAG, "Event published successfully!");
            _eventPublished = true;
            _hasPendingEvent = false;
        }
        else
        {
            LOG_WARNING(TAG, "Event publish failed, will retry...");
        }
    }
}

// ============================================================
//  Event History (NVS — Circular Buffer of 5)
// ============================================================

static const char* EVENTS_NAMESPACE = "events";

void SystemManager::saveEventToHistory(const DeliveryRecord& record, const String& unit)
{
    Preferences prefs;
    prefs.begin(EVENTS_NAMESPACE, false);

    // Read current write index and count
    uint8_t wrIdx = prefs.getUChar("wr_idx", 0);
    uint8_t count = prefs.getUChar("count", 0);

    // Serialize event to a compact JSON string
    char eventJson[160];
    StaticJsonDocument<160> doc;
    doc["s"] = record.startTime;
    doc["e"] = record.endTime;
    doc["v"] = record.totalLiters;
    doc["u"] = unit;
    serializeJson(doc, eventJson, sizeof(eventJson));

    // Store at current write position
    char key[4];
    snprintf(key, sizeof(key), "e%d", wrIdx);
    prefs.putString(key, eventJson);

    // Advance write index (circular)
    wrIdx = (wrIdx + 1) % MAX_EVENT_HISTORY;
    prefs.putUChar("wr_idx", wrIdx);

    // Update count (max 5)
    if (count < MAX_EVENT_HISTORY)
    {
        count++;
        prefs.putUChar("count", count);
    }

    prefs.end();

    LOG_INFO(TAG, "Event saved to history (slot %d, total %d)",
             (wrIdx == 0 ? MAX_EVENT_HISTORY - 1 : wrIdx - 1), count);
}

void SystemManager::publishEventHistory()
{
    if (!_mqtt.isConnected())
    {
        LOG_WARNING(TAG, "Cannot publish history — MQTT not connected");
        return;
    }

    Preferences prefs;
    prefs.begin(EVENTS_NAMESPACE, true);  // Read-only

    uint8_t count = prefs.getUChar("count", 0);
    uint8_t wrIdx = prefs.getUChar("wr_idx", 0);

    LOG_INFO(TAG, "Publishing event history (%d events)", count);

    // Build JSON array of events
    StaticJsonDocument<1024> doc;
    doc["event"] = "history";
    doc["count"] = count;
    JsonArray events = doc.createNestedArray("events");

    // Read events in chronological order (oldest first)
    for (uint8_t i = 0; i < count; i++)
    {
        // Calculate the read index (oldest event first)
        uint8_t readIdx;
        if (count < MAX_EVENT_HISTORY)
        {
            readIdx = i;  // Haven't wrapped yet
        }
        else
        {
            readIdx = (wrIdx + i) % MAX_EVENT_HISTORY;  // Start from oldest
        }

        char key[4];
        snprintf(key, sizeof(key), "e%d", readIdx);
        String eventJson = prefs.getString(key, "");

        if (eventJson.length() > 0)
        {
            StaticJsonDocument<128> eventDoc;
            if (deserializeJson(eventDoc, eventJson) == DeserializationError::Ok)
            {
                JsonObject ev = events.createNestedObject();
                ev["start"]  = eventDoc["s"].as<String>();
                ev["stop"]   = eventDoc["e"].as<String>();
                ev["volume"] = eventDoc["v"].as<float>();
                ev["unit"]   = eventDoc["u"].as<String>();
            }
        }
    }

    prefs.end();

    // Publish
    char buffer[1024];
    size_t written = serializeJson(doc, buffer, sizeof(buffer));
    if (written > 0 && written < sizeof(buffer))
    {
        char topic[MQTTManager::MAX_TOPIC_LEN];
        _mqtt.getTelemetryTopic(topic, sizeof(topic));
        _mqtt.publish(topic, buffer);
        LOG_INFO(TAG, "Event history published");
    }
}

// ============================================================
//  JSON Serialization
// ============================================================

bool SystemManager::serializeEvent(const DeliveryRecord& record,
                                    char* buffer, size_t len)
{
    StaticJsonDocument<256> doc;

    doc["event"]         = "delivery";
    doc["start"]         = record.startTime;
    doc["stop"]          = record.endTime;
    doc["volume"]        = record.totalLiters;
    doc["unit"]          = _flowModbus.getUnitStr();

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

        if (strcmp(cmd, "CLEAR") == 0)
        {
            _flowModbus.clearCumulative();
            LOG_INFO(TAG, "Cumulative flow cleared via MQTT command");
        }
        else if (strcmp(cmd, "LAST") == 0)
        {
            LOG_INFO(TAG, "LAST command received — publishing event history");
            publishEventHistory();
        }
        else if (strcmp(cmd, "status") == 0)
        {
            LOG_INFO(TAG, "Status command received");
            // Minimal status response
            char buffer[256];
            StaticJsonDocument<256> statusDoc;
            statusDoc["device_id"] = MQTT_DEVICE_ID;
            statusDoc["online"]    = true;
            statusDoc["uptime_s"]  = millis() / 1000;
            statusDoc["free_heap"] = ESP.getFreeHeap();

            size_t written = serializeJson(statusDoc, buffer, sizeof(buffer));
            if (written > 0)
            {
                char statusTopic[MQTTManager::MAX_TOPIC_LEN];
                _mqtt.getTelemetryTopic(statusTopic, sizeof(statusTopic));
                _mqtt.publish(statusTopic, buffer);
            }
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
//  Deep Sleep
// ============================================================

void SystemManager::checkSleepCondition()
{
    // Do NOT sleep if there is a pending event that hasn't been published
    if (_hasPendingEvent && !_eventPublished)
    {
        return;
    }

    unsigned long now = millis();
    unsigned long idleDuration = now - _lastFlowDetectedMs;
    unsigned long idleSeconds = idleDuration / 1000;

    // Case 1: Flow was detected since wake-up, now it stopped.
    // Wait for the grace period before sleeping.
    if (_flowDetectedSinceWakeup)
    {
        if (idleSeconds >= DEEP_SLEEP_GRACE_PERIOD_S)
        {
            LOG_INFO(TAG, "No flow for %lu seconds (grace period elapsed). Entering deep sleep...",
                     idleSeconds);
            enterDeepSleep();
        }
        return;
    }

    // Case 2: No flow detected since wake-up (false trigger / noise).
    // Use the idle timeout to avoid staying awake forever.
    if (idleSeconds >= DEEP_SLEEP_IDLE_TIMEOUT_S)
    {
        LOG_INFO(TAG, "No flow detected for %lu seconds since wake-up. Returning to deep sleep...",
                 idleSeconds);
        enterDeepSleep();
    }
}

void SystemManager::enterDeepSleep()
{
    LOG_INFO(TAG, "====================================");
    LOG_INFO(TAG, "  Entering Deep Sleep");
    LOG_INFO(TAG, "====================================");

    // Publish a final "going to sleep" status if MQTT is connected
    if (_mqtt.isConnected())
    {
        char buffer[256];
        StaticJsonDocument<256> doc;
        doc["device_id"] = MQTT_DEVICE_ID;
        doc["event"]     = "deep_sleep";
        doc["uptime_s"]  = millis() / 1000;

        size_t written = serializeJson(doc, buffer, sizeof(buffer));
        if (written > 0)
        {
            char topic[MQTTManager::MAX_TOPIC_LEN];
            _mqtt.getTelemetryTopic(topic, sizeof(topic));
            _mqtt.publish(topic, buffer);
        }

        // Give MQTT time to send the message
        delay(200);

        // Disconnect MQTT gracefully
        _mqtt.disconnect();
    }

    // Disconnect WiFi
    _wifi.disconnect();

    // Remove watchdog before sleeping (otherwise it triggers a reboot)
    esp_task_wdt_delete(NULL);

    // Flush serial output
    Serial.flush();
    delay(100);

    LOG_INFO(TAG, "Good night! Waiting for flow pulse on GPIO %d...", (int)WAKEUP_PIN);
    Serial.flush();

    // Enter deep sleep — only ext0 wake-up will bring us back
    esp_deep_sleep_start();
}
