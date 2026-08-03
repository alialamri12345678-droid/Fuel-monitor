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

    // Read 14 holding registers starting at address 0x0000
    if (!_modbus.readHoldingRegisters(0, 14))
    {
        _data.valid = false;
        LOG_WARNING(TAG, "Failed to read holding registers");
        return false;
    }

    // Decode floats (byte order 3412 = register word swap)
    _data.temperature = decodeFloat3412(
        _modbus.getRegister(0), _modbus.getRegister(1));

    _data.flowRate = decodeFloat3412(
        _modbus.getRegister(2), _modbus.getRegister(3));

    _data.velocity = decodeFloat3412(
        _modbus.getRegister(4), _modbus.getRegister(5));

    _data.frequency = decodeFloat3412(
        _modbus.getRegister(6), _modbus.getRegister(7));

    _data.cumulativeHigh = decodeFloat3412(
        _modbus.getRegister(8), _modbus.getRegister(9));

    _data.cumulativeLow = decodeFloat3412(
        _modbus.getRegister(10), _modbus.getRegister(11));

    _data.flowUnitCode = decodeFloat3412(
        _modbus.getRegister(12), _modbus.getRegister(13));

    // Calculate total cumulative flow
    _data.totalFlow = (_data.cumulativeHigh * 100.0f) + _data.cumulativeLow;
    
    // Parse unit
    _data.flowUnitStr = getUnitString(_data.flowUnitCode);

    _data.valid = true;

    // Print parsed values
    LOG_DEBUG(TAG, "Temperature:        %.1f °C", _data.temperature);
    LOG_DEBUG(TAG, "Instantaneous Flow: %.3f %s", _data.flowRate, _data.flowUnitStr.c_str());
    LOG_DEBUG(TAG, "Flow Velocity:      %.2f m/s", _data.velocity);
    LOG_DEBUG(TAG, "Frequency:          %.1f Hz", _data.frequency);
    LOG_DEBUG(TAG, "Cumulative Flow:    %.3f m³", _data.totalFlow);
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
    // If unit is already L/min, return it directly. 
    // Otherwise assume m³/h and convert.
    int code = (int)(_data.flowUnitCode + 0.5f);
    if (code == 1) return _data.flowRate; 
    
    // Convert m³/h to L/min
    return _data.flowRate * 16.6667f;
}

double FlowMeterModbus::getCumulativeLiters() const
{
    // Convert m³ to Liters
    return (double)_data.totalFlow * 1000.0;
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

    // Function 04 is Input Registers (read), but the manual says 
    // "Function 04 - Write 0x0000 0x0001 to clear".
    // Usually writing is Function 16 or 6. If they literally mean F04, 
    // ModbusMaster doesn't support writing via F04 natively. 
    // We will attempt Function 16 (Write Multiple Registers) 
    // since that is standard for clearing.
    uint16_t clearCmd[2] = {0x0000, 0x0001};
    if (!_modbus.writeMultipleRegisters(0, 2, clearCmd))
    {
        LOG_WARNING(TAG, "Failed to clear cumulative flow");
        return false;
    }

    LOG_INFO(TAG, "Cumulative flow cleared");
    return true;
}
