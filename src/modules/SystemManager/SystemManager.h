#ifndef SYSTEM_MANAGER_H
#define SYSTEM_MANAGER_H

#include <Arduino.h>
#include <esp_task_wdt.h>

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
 * ============================================================
 */

class SystemManager
{
public:

    SystemManager();

    /**
     * @brief Initialize all modules. Called once in setup().
     * @return true if all critical modules initialized.
     */
    bool begin();

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

    // Session total NVS persistence
    void loadSessionTotal();
    void saveSessionTotal();

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
    //  JSON Serialization
    // ========================================================

    bool serializeLiveData(char* buffer, size_t len);
    bool serializeDelivery(const DeliveryRecord& record,
                           char* buffer, size_t len);
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
