#ifndef SYSTEM_MANAGER_H
#define SYSTEM_MANAGER_H

#include <Arduino.h>
#include <esp_task_wdt.h>
#include <esp_sleep.h>

#include "../DeliveryManager/DeliveryManager.h"
#include "../WiFiManager/WiFiManager.h"
#include "../MQTTManager/MQTTManager.h"
#include "../ModbusManager/ModbusManager.h"
#include "../FlowMeterModbus/FlowMeterModbus.h"

#include "../../config/Config.h"

/**
 * ============================================================
 * SystemManager
 * ------------------------------------------------------------
 * Top-level orchestrator for the Diesel Delivery Monitoring
 * System.
 *
 * Responsibilities:
 *  - Initialize all modules in correct dependency order
 *  - Run the main non-blocking loop with millis-based scheduling
 *  - Event-based reporting: publish once per delivery event
 *  - Store last 5 events in NVS
 *  - Manage deep sleep for battery conservation
 * ============================================================
 */

class SystemManager
{
public:

    SystemManager();

    /**
     * @brief Initialize all modules. Called once in setup().
     * @param wakeupCause The reason the ESP32 woke up.
     * @return true if all critical modules initialized.
     */
    bool begin(esp_sleep_wakeup_cause_t wakeupCause = ESP_SLEEP_WAKEUP_UNDEFINED);

    /**
     * @brief Main non-blocking update loop. Called in loop().
     */
    void update();

private:

    // ========================================================
    //  Module Instances
    // ========================================================

    DeliveryManager _delivery;
    WiFiManager     _wifi;
    MQTTManager     _mqtt;

    ModbusManager     _modbus;
    FlowMeterModbus   _flowModbus;
    unsigned long     _lastModbusPoll;

    // ========================================================
    //  System State
    // ========================================================

    bool _initialized;
    bool _mqttSubscribed;

    // Wake-up cause (deep sleep vs power-on)
    esp_sleep_wakeup_cause_t _wakeupCause;

    // ========================================================
    //  Event Tracking
    // ========================================================

    // Pending event waiting to be published
    DeliveryRecord _pendingEvent;
    bool           _hasPendingEvent;
    bool           _eventPublished;

    // ========================================================
    //  Deep Sleep State
    // ========================================================

    unsigned long _lastFlowDetectedMs;
    bool          _flowDetectedSinceWakeup;

    // ========================================================
    //  Scheduling Timers (millis-based)
    // ========================================================

    unsigned long _lastSensorRead;
    unsigned long _lastSerialLog;
    // ========================================================
    //  Initialization Helpers
    // ========================================================

    bool initializeModules();
    void connectNetwork();
    void connectMQTT();
    void subscribeTopics();

    // ========================================================
    //  Scheduling
    // ========================================================

    static bool timerExpired(unsigned long& lastTime,
                             unsigned long interval);

    // ========================================================
    //  Data Processing
    // ========================================================

    void readSensors();
    void handleDeliveryCompletion();
    void publishPendingEvent();

    // ========================================================
    //  Event History (NVS)
    // ========================================================

    void saveEventToHistory(const DeliveryRecord& record, const String& unit);
    void publishEventHistory();

    // ========================================================
    //  Deep Sleep
    // ========================================================

    void checkSleepCondition();
    void enterDeepSleep();

    // ========================================================
    //  JSON Serialization
    // ========================================================

    bool serializeEvent(const DeliveryRecord& record, char* buffer, size_t len);

    // ========================================================
    //  MQTT Callback
    // ========================================================

    static SystemManager* _instance;

    static void mqttCallbackStatic(
        char* topic,
        uint8_t* payload,
        unsigned int length);

    void handleMQTTMessage(
        const char* topic,
        const uint8_t* payload,
        unsigned int length);
};

#endif // SYSTEM_MANAGER_H
