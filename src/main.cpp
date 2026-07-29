#include <Arduino.h>
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
 * ============================================================
 */

SystemManager systemManager;

void setup()
{
    systemManager.begin();
}

void loop()
{
    systemManager.update();
}
