#include "FlowMeterModbus.h"
#include "../../utils/Logger.h"

static const char* TAG = "FlowModbus";

// ============================================================
//  Helper: 3412 Byte Order Decoder
// ============================================================

float FlowMeterModbus::decodeFloat3412(uint16_t regHi, uint16_t regLo)
{
    // The ModbusMaster library reads words as uint16_t.
    // In 3412 byte order, we just need to swap the two 16-bit registers
    // before copying them into a 32-bit float.
    union {
        uint32_t i;
        float f;
    } u;

    // Place the first register in the lower 16 bits,
    // and the second register in the upper 16 bits.
    u.i = ((uint32_t)regLo << 16) | regHi;
    return u.f;
}

String FlowMeterModbus::getUnitString(float unitCode)
{
    int code = (int)(unitCode + 0.5f); // Round float to nearest int
    switch (code) {
        case 0: return "m³/h";
        case 1: return "L/min";
        case 2: return "kg/h";
        case 3: return "L/h";
        case 4: return "T/h";
        case 5: return "kg/min";
        case 6: return "m³/min";
        default: return "Unknown";
    }
}

// ============================================================
//  Lifecycle
// ============================================================

FlowMeterModbus::FlowMeterModbus(ModbusManager& modbus)
    : _modbus(modbus), _slaveID(1)
{
    memset(&_data, 0, sizeof(_data));
}

void FlowMeterModbus::begin(uint8_t slaveID)
{
    _slaveID = slaveID;
    LOG_INFO(TAG, "Initialized for Modbus slave ID %d", _slaveID);
}

// ============================================================
//  Polling
// ============================================================

bool FlowMeterModbus::update()
{
    _modbus.setSlaveID(_slaveID);

    // Read 4 holding registers starting at address 0x0008 (Cumulative Flow High/Low)
    if (!_modbus.readHoldingRegisters(8, 4))
    {
        _data.valid = false;
        LOG_WARNING(TAG, "Failed to read holding registers");
        return false;
    }

    // Decode floats (byte order 3412 = register word swap)
    _data.cumulativeHigh = decodeFloat3412(
        _modbus.getRegister(0), _modbus.getRegister(1));

    _data.cumulativeLow = decodeFloat3412(
        _modbus.getRegister(2), _modbus.getRegister(3));

    // Clear unused fields
    _data.temperature = 0.0f;
    _data.flowRate = 0.0f;
    _data.velocity = 0.0f;
    _data.frequency = 0.0f;
    _data.flowUnitCode = 0.0f;
    _data.flowUnitStr = "L"; // Sensor configured for liters

    // Calculate total cumulative flow (already in liters)
    _data.totalLiters = (_data.cumulativeHigh * 100.0f) + _data.cumulativeLow;
    
    _data.valid = true;

    // Print parsed values
    LOG_DEBUG(TAG, "Temperature:        %.1f °C", _data.temperature);
    LOG_DEBUG(TAG, "Instantaneous Flow: %.3f %s", _data.flowRate, _data.flowUnitStr.c_str());
    LOG_DEBUG(TAG, "Flow Velocity:      %.2f m/s", _data.velocity);
    LOG_DEBUG(TAG, "Frequency:          %.1f Hz", _data.frequency);
    LOG_DEBUG(TAG, "Cumulative Flow:    %.3f L", _data.totalLiters);
    LOG_DEBUG(TAG, "Flow Unit:          %s", _data.flowUnitStr.c_str());

    return true;
}

// ============================================================
//  Accessors
// ============================================================

const FlowMeterModbus::MeterData& FlowMeterModbus::getData() const
{
    return _data;
}

float FlowMeterModbus::getFlowLPM() const
{
    // Sensor is configured to output directly in L/min — no conversion needed
    return _data.flowRate;
}

double FlowMeterModbus::getCumulativeLiters() const
{
    // Sensor is configured to output directly in liters — no conversion needed
    return (double)_data.totalLiters;
}

bool FlowMeterModbus::isOnline() const
{
    return _modbus.isConnected() && _data.valid;
}

// ============================================================
//  Clear Cumulative Total
// ============================================================

bool FlowMeterModbus::clearCumulative()
{
    _modbus.setSlaveID(_slaveID);

    LOG_INFO(TAG, "--- ATTEMPTING CUMULATIVE CLEAR ---");
    
    // Manufacturer's updated clear command:
    // Address: 0x0013 (Decimal 19)
    // Function code: 06 (Standard Write Single Register)
    // Data: unsigned integer 99 = 0x0063
    // Raw Frame provided by manufacturer: 01 06 00 13 00 63 38 26
    
    LOG_INFO(TAG, "Sending clear command (FC06 to addr 0x0013, val 99)...");

    if (_modbus.writeSingleRegister(0x0013, 99))
    {
        LOG_INFO(TAG, "Clear command (99 to 0x0013) acknowledged by flowmeter!");
        return true;
    }

    LOG_WARNING(TAG, "Clear command (99 to 0x0013): no response from flowmeter");
    return false;
}
