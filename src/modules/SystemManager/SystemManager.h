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
 *  - Route data between modules
 *  - Handle MQTT callbacks and command routing
 *  - Manage deep sleep for battery conservation
 * ============================================================
 */

class SystemManager
{
public:

    SystemManager();

    /**
     * @brief Initialize all modules. Called once in setup().
     * @param wakeupCause The reason the ESP32 woke up (deep sleep or power-on).
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

    // Cumulative total liters (persisted to NVS across reboots)
    float _sessionTotalLiters;

    // Sequence number for HMAC-signed payloads (persisted to NVS)
    unsigned long _sequenceNumber;

    // Wake-up cause (deep sleep vs power-on)
    esp_sleep_wakeup_cause_t _wakeupCause;

    // ========================================================
    //  Deep Sleep State
    // ========================================================

    // Timestamp of the last detected non-zero flow
    unsigned long _lastFlowDetectedMs;

    // Whether any flow has been detected since wake-up
    bool _flowDetectedSinceWakeup;

    // ========================================================
    //  Scheduling Timers (millis-based)
    // ========================================================

    unsigned long _lastSensorRead;
    unsigned long _lastMqttPublish;
    unsigned long _lastStatusPublish;

    // ========================================================
    //  Initialization Helpers
    // ========================================================

    bool initializeModules();
    void connectNetwork();
    void connectMQTT();
    void subscribeTopics();

    // NVS persistence
    void loadSessionTotal();
    void saveSessionTotal();
    void loadSequenceNumber();
    void saveSequenceNumber();

    // ========================================================
    //  Scheduling
    // ========================================================

    static bool timerExpired(unsigned long& lastTime,
                             unsigned long interval);

    // ========================================================
    //  Data Processing
    // ========================================================

    void readSensors();
    void publishLiveData();
    void publishStatus();
    void publishDeliveryRecord(const DeliveryRecord& record);
    void handleDeliveryCompletion();

    // ========================================================
    //  Deep Sleep
    // ========================================================

    void checkSleepCondition();
    void enterDeepSleep();

    // ========================================================
    //  JSON Serialization
    // ========================================================

    bool serializeTelemetry(char* buffer, size_t len);
    bool serializeStatus(char* buffer, size_t len);

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
