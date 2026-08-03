#include "ModbusManager.h"
#include "../../utils/Logger.h"
#include <esp_task_wdt.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char* TAG = "Modbus";

ModbusManager* ModbusManager::instance = nullptr;

ModbusManager::ModbusManager(HardwareSerial& serial,
                             uint8_t derePin)
    : _serial(serial),
      _derePin(derePin),
      _slaveID(1),
      _connected(false),
      _retryCount(2),
      _consecutiveFailures(0),
      _failureThreshold(5),
      _stats()
{
    instance = this;
}

static void modbusIdle() {
    esp_task_wdt_reset();
    vTaskDelay(pdMS_TO_TICKS(1));
}

bool ModbusManager::begin(uint32_t baudRate,
                          uint32_t config)
{
    pinMode(_derePin, OUTPUT);
    digitalWrite(_derePin, LOW);

    _serial.begin(baudRate, config);

    _modbus.begin(_slaveID, _serial);

    _modbus.preTransmission(preTransmission);
    _modbus.postTransmission(postTransmission);
    _modbus.idle(modbusIdle);

    LOG_INFO(TAG, "Initialized (baud=%lu, slave=%u)",
             (unsigned long)baudRate, _slaveID);

    return true;
}

void ModbusManager::setSlaveID(uint8_t slaveID)
{
    _slaveID = slaveID;
    _modbus.begin(_slaveID, _serial);

    LOG_DEBUG(TAG, "Slave ID set to %u", _slaveID);
}

uint8_t ModbusManager::getSlaveID() const
{
    return _slaveID;
}

/*==================================================
    Read Functions
==================================================*/

bool ModbusManager::readHoldingRegisters(uint16_t address,
                                         uint16_t quantity)
{
    for (uint8_t attempt = 0; attempt <= _retryCount; attempt++)
    {
        uint8_t result =
            _modbus.readHoldingRegisters(address, quantity);

        updateStatistics(result);

        if (result == _modbus.ku8MBSuccess)
        {
            return true;
        }

        if (attempt < _retryCount)
        {
            LOG_DEBUG(TAG,
                      "Retry %u/%u for register 0x%04X",
                      attempt + 1, _retryCount, address);
            vTaskDelay(pdMS_TO_TICKS(5));
        }
    }

    return false;
}

bool ModbusManager::readInputRegisters(uint16_t address,
                                       uint16_t quantity)
{
    for (uint8_t attempt = 0; attempt <= _retryCount; attempt++)
    {
        uint8_t result =
            _modbus.readInputRegisters(address, quantity);

        updateStatistics(result);

        if (result == _modbus.ku8MBSuccess)
        {
            return true;
        }

        if (attempt < _retryCount)
        {
            vTaskDelay(pdMS_TO_TICKS(5));
        }
    }

    return false;
}

/*==================================================
    Write Functions
==================================================*/

bool ModbusManager::writeSingleRegister(uint16_t address,
                                        uint16_t value)
{
    for (uint8_t attempt = 0; attempt <= _retryCount; attempt++)
    {
        uint8_t result =
            _modbus.writeSingleRegister(address, value);

        updateStatistics(result);

        if (result == _modbus.ku8MBSuccess)
        {
            return true;
        }

        if (attempt < _retryCount)
        {
            vTaskDelay(pdMS_TO_TICKS(5));
        }
    }

    return false;
}

bool ModbusManager::writeMultipleRegisters(uint16_t address,
                                           uint16_t quantity,
                                           uint16_t* values)
{
    for (uint16_t i = 0; i < quantity; i++)
    {
        _modbus.setTransmitBuffer(i, values[i]);
    }

    for (uint8_t attempt = 0; attempt <= _retryCount; attempt++)
    {
        uint8_t result =
            _modbus.writeMultipleRegisters(address, quantity);

        updateStatistics(result);

        if (result == _modbus.ku8MBSuccess)
        {
            return true;
        }

        if (attempt < _retryCount)
        {
            // Reload transmit buffer for retry
            for (uint16_t i = 0; i < quantity; i++)
            {
                _modbus.setTransmitBuffer(i, values[i]);
            }

            vTaskDelay(pdMS_TO_TICKS(5));
        }
    }

    return false;
}

/*==================================================
    Response Access
==================================================*/

uint16_t ModbusManager::getRegister(uint8_t index)
{
    return _modbus.getResponseBuffer(index);
}

/*==================================================
    Status
==================================================*/

bool ModbusManager::isConnected() const
{
    return _connected;
}

ModbusManager::Statistics
ModbusManager::getStatistics() const
{
    return _stats;
}

void ModbusManager::resetStatistics()
{
    _stats.reset();
}

void ModbusManager::setRetryCount(uint8_t retries)
{
    _retryCount = retries;
}

void ModbusManager::setConsecutiveFailureThreshold(uint8_t threshold)
{
    _failureThreshold = threshold;
}

/*==================================================
    RS485 Direction Control
==================================================*/

void ModbusManager::preTransmission()
{
    if (instance)
    {
        digitalWrite(instance->_derePin, HIGH);
    }
}

void ModbusManager::postTransmission()
{
    if (instance)
    {
        digitalWrite(instance->_derePin, LOW);
    }
}

/*==================================================
    Statistics & Connection Tracking
==================================================*/

void ModbusManager::updateStatistics(uint8_t result)
{
    if (result == _modbus.ku8MBSuccess)
    {
        _stats.successfulRequests++;
        _consecutiveFailures = 0;

        if (!_connected)
        {
            _connected = true;
            LOG_INFO(TAG, "Communication restored");
        }
    }
    else
    {
        _stats.failedRequests++;
        _consecutiveFailures++;

        switch (result)
        {
            case ModbusMaster::ku8MBResponseTimedOut:
                _stats.timeoutErrors++;
                LOG_WARNING(TAG,
                            "Response timeout (consecutive: %u)",
                            _consecutiveFailures);
                break;

            case ModbusMaster::ku8MBInvalidCRC:
                _stats.crcErrors++;
                LOG_WARNING(TAG, "CRC error");
                break;

            default:
                LOG_WARNING(TAG,
                            "Error code: 0x%02X",
                            result);
                break;
        }

        if (_connected &&
            _consecutiveFailures >= _failureThreshold)
        {
            _connected = false;
            LOG_ERROR(TAG,
                      "Device offline after %u consecutive failures",
                      _consecutiveFailures);
        }
    }
}
