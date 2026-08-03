#ifndef MODBUS_MANAGER_H
#define MODBUS_MANAGER_H

#include <Arduino.h>
#include <ModbusMaster.h>

/**
 * ============================================================
 * ModbusManager
 * ------------------------------------------------------------
 * RS485 Modbus RTU driver for ESP32.
 *
 * Adapted from the Generator Monitoring & Control project.
 *
 * Features:
 *  - Initialize RS485 transceiver (DE/RE pin)
 *  - Read holding registers, input registers
 *  - Write single/multiple registers
 *  - Automatic retry on failure
 *  - Communication statistics tracking
 *  - Connection status with hysteresis
 * ============================================================
 */

class ModbusManager
{
public:

    struct Statistics
    {
        uint32_t successfulRequests = 0;
        uint32_t failedRequests     = 0;
        uint32_t timeoutErrors      = 0;
        uint32_t crcErrors          = 0;

        void reset()
        {
            successfulRequests = 0;
            failedRequests     = 0;
            timeoutErrors      = 0;
            crcErrors          = 0;
        }
    };

    /**
     * @brief Constructor.
     * @param serial   HardwareSerial port (e.g. Serial2).
     * @param derePin  DE/RE control pin for RS485 transceiver.
     */
    ModbusManager(HardwareSerial& serial = Serial2,
                  uint8_t derePin = 4);

    /**
     * @brief Initialize Modbus communication.
     * @param baudRate Serial baud rate.
     * @param config   Serial config (default SERIAL_8N1).
     */
    bool begin(uint32_t baudRate = 9600,
               uint32_t config = SERIAL_8N1);

    /**
     * @brief Set the Modbus slave ID.
     */
    void setSlaveID(uint8_t slaveID);

    /**
     * @brief Get current slave ID.
     */
    uint8_t getSlaveID() const;

    /*============================
      Read Functions
    ============================*/

    bool readHoldingRegisters(uint16_t address,
                              uint16_t quantity);

    bool readInputRegisters(uint16_t address,
                            uint16_t quantity);

    /*============================
      Write Functions
    ============================*/

    bool writeSingleRegister(uint16_t address,
                             uint16_t value);

    bool writeMultipleRegisters(uint16_t address,
                                uint16_t quantity,
                                uint16_t* values);

    /*============================
      Response Access
    ============================*/

    uint16_t getRegister(uint8_t index);

    /*============================
      Status
    ============================*/

    bool isConnected() const;

    Statistics getStatistics() const;

    void resetStatistics();

    /*============================
      Configuration
    ============================*/

    void setRetryCount(uint8_t retries);

    void setConsecutiveFailureThreshold(uint8_t threshold);

private:

    HardwareSerial& _serial;

    ModbusMaster _modbus;

    uint8_t _derePin;

    uint8_t _slaveID;

    bool _connected;

    uint8_t _retryCount;

    uint8_t _consecutiveFailures;

    uint8_t _failureThreshold;

    Statistics _stats;

    /*============================
      RS485 Direction Control
    ============================*/

    static ModbusManager* instance;

    static void preTransmission();

    static void postTransmission();

    /*============================
      Internal
    ============================*/

    void updateStatistics(uint8_t result);
};

#endif // MODBUS_MANAGER_H
