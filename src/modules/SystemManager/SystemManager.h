#ifndef SYSTEM_MANAGER_H
#define SYSTEM_MANAGER_H

#include <Arduino.h>
#include <esp_task_wdt.h>

#include "../FlowMeter/FlowMeter.h"
#include "../DeliveryManager/DeliveryManager.h"
#include "../RTCManager/RTCManager.h"
#include "../SDLogger/SDLogger.h"
#include "../WiFiManager/WiFiManager.h"
#include "../MQTTManager/MQTTManager.h"
#include "../WebDashboard/WebDashboard.h"

/**
 * ============================================================
 * SystemManager
 * ------------------------------------------------------------
 * Top-level orchestrator for the Diesel Delivery Verification
 * & Monitoring System.
 *
 * Responsibilities:
 *  - Initialize all modules in correct dependency order
 *  - Run the main non-blocking loop with millis-based scheduling
 *  - Route data between modules
 *  - Handle MQTT callbacks and command routing
 *  - Manage offline data synchronization
 *  - NTP time synchronization
 *
 * SystemManager does NOT:
 *  - Contain signal processing logic (FlowMeter)
 *  - Contain delivery detection logic (DeliveryManager)
 *  - Build JSON payloads directly (uses helper methods)
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

    FlowMeter       _flowMeter;
    RTCManager      _rtc;
    DeliveryManager _delivery;
    SDLogger        _sdLogger;
    WiFiManager     _wifi;
    MQTTManager     _mqtt;
    WebDashboard    _dashboard;

    // ========================================================
    //  System State
    // ========================================================

    bool _initialized;
    bool _ntpSynced;
    bool _ntpRequested;     // NTP configTzTime() called, waiting for sync
    bool _mqttSubscribed;

    // Cumulative total liters (persisted to NVS across reboots)
    float _sessionTotalLiters;

    // ========================================================
    //  Scheduling Timers (millis-based)
    // ========================================================

    unsigned long _lastSensorRead;
    unsigned long _lastLogWrite;
    unsigned long _lastMqttPublish;
    unsigned long _lastStatusPublish;
    unsigned long _lastOfflineSync;
    unsigned long _lastDashboardCleanup;

    // ========================================================
    //  Initialization Helpers
    // ========================================================

    bool initializeModules();
    void connectNetwork();
    void startNTPSync();
    void checkNTPSync();
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
    void logToSD();
    void publishLiveData();
    void publishStatus();
    void publishDeliveryRecord(const DeliveryRecord& record);
    void handleDeliveryCompletion();
    void syncPendingDeliveries();
    void pushDashboardData();

    // Dashboard command callback
    static void dashboardCommandHandler(const char* command);

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
