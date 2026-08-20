#ifndef FLOW_METER_MODBUS_H
#define FLOW_METER_MODBUS_H

#include <Arduino.h>
#include "../ModbusManager/ModbusManager.h"

/**
 * ============================================================
 * FlowMeterModbus
 * ------------------------------------------------------------
 * Reads turbine flowmeter data via Modbus RTU (RS485).
 *
 * Register Map (Function 03 — Holding Registers):
 * ┌────────┬──────────────────┬───────┬───────────────────────┐
 * │ Reg    │ Parameter        │ Type  │ Unit                  │
 * ├────────┼──────────────────┼───────┼───────────────────────┤
 * │ 0-1    │ Temperature      │ Float │ °C                    │
 * │ 2-3    │ Inst. Flow       │ Float │ L/min                 │
 * │ 4-5    │ Flow Velocity    │ Float │ m/s                   │
 * │ 6-7    │ Frequency        │ Float │ Hz                    │
 * │ 8-9    │ Cumul. Flow High │ Float │ Liters (high part)    │
 * │ 10-11  │ Cumul. Flow Low  │ Float │ Liters (low part)     │
 * │ 12-13  │ Flow Unit        │ Float │ Unit code             │
 * └────────┴──────────────────┴───────┴───────────────────────┘
 *
 * Float byte order: 3412 (register word swap)
 * NOTE: Sensor is configured to output directly in Liters.
 * ============================================================
 */

class FlowMeterModbus
{
public:

    struct MeterData
    {
        float temperature;      // °C
        float flowRate;         // L/min (sensor configured for liters)
        float velocity;         // m/s
        float frequency;        // Hz
        float cumulativeHigh;   // High part of cumulative (liters)
        float cumulativeLow;    // Low part of cumulative (liters)
        float totalLiters;      // Calculated: high * 100 + low (liters)
        float flowUnitCode;     
        String flowUnitStr;     // Readable string
        bool  valid;            // true if last read succeeded
    };

    /**
     * @brief Constructor.
     * @param modbus  Reference to shared ModbusManager.
     */
    FlowMeterModbus(ModbusManager& modbus);

    /**
     * @brief Initialize the flow meter with slave address.
     * @param slaveID  Modbus slave address of the transmitter.
     */
    void begin(uint8_t slaveID = 1);

    /**
     * @brief Poll all 14 holding registers from the transmitter.
     *        Call at 1 Hz (every 1 second) from the main loop.
     * @return true if read succeeded.
     */
    bool update();

    /**
     * @brief Get the latest parsed meter data.
     */
    const MeterData& getData() const;

    /**
     * @brief Send clear/reset command to the transmitter
     */
    bool clearCumulative();

    /**
     * @brief Get flow rate in L/min (direct from sensor, no conversion).
     */
    float getFlowLPM() const;

    /**
     * @brief Get total cumulative flow (direct from sensor, no conversion).
     */
    double getCumulativeLiters() const;

    /**
     * @brief Get the unit string read from the sensor (e.g. "L", "m3", "kg").
     */
    const String& getUnitStr() const;

    /**
     * @brief Check if communication is active.
     */
    bool isOnline() const;

private:

    ModbusManager& _modbus;
    uint8_t  _slaveID;
    MeterData _data;

    /**
     * @brief Decode a 32-bit float from two 16-bit registers
     *        using byte order 3412 (word swap).
     */
    static float decodeFloat3412(uint16_t regHi, uint16_t regLo);
    
    /**
     * @brief Convert flow unit float code to a readable string
     */
    static String getUnitString(float unitCode);
};

#endif // FLOW_METER_MODBUS_H
