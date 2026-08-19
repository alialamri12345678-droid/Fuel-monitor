#include <Arduino.h>
#include <esp_sleep.h>
#include "modules/SystemManager/SystemManager.h"

/**
 * ============================================================
 * Diesel Delivery Verification & Monitoring System
 * ============================================================
 *
 * Firmware entry point.
 *
 * All logic is encapsulated in the SystemManager class.
 * This file simply creates the instance and delegates
 * setup() and loop() to it.
 *
 * Deep Sleep:
 *   The ESP32 enters deep sleep when no flow is detected
 *   for a configurable grace period. It wakes on an external
 *   pulse signal (ext0) from the flowmeter's frequency output.
 * ============================================================
 */

SystemManager systemManager;

void setup()
{
    esp_sleep_wakeup_cause_t wakeupCause = esp_sleep_get_wakeup_cause();
    systemManager.begin(wakeupCause);
}

void loop()
{
    systemManager.update();
}
