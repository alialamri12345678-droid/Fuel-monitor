#ifndef SD_LOGGER_H
#define SD_LOGGER_H

#include <Arduino.h>
#include <SD.h>
#include "../../utils/DataTypes.h"

/**
 * ============================================================
 * SDLogger
 * ------------------------------------------------------------
 * Manages SD card storage for the Diesel Delivery Monitoring
 * System.
 *
 * Creates and maintains two CSV files:
 *   1. flow_log.csv      — Continuous 1-second measurement log
 *   2. delivery_summary.csv — One row per completed delivery
 *
 * Also manages a binary file for pending (offline) delivery
 * records that have not yet been uploaded via MQTT.
 *
 * Features:
 *  - Auto-create files with CSV headers
 *  - Safe append with flush (power-loss resilient)
 *  - SD card remount on failure with backoff
 *  - Thread-safe SD access via FreeRTOS mutex
 * ============================================================
 */

class SDLogger
{
public:

    SDLogger();

    /**
     * @brief Initialize SD card on the specified CS pin.
     * @param csPin SPI chip select pin.
     * @return true if SD card mounted successfully.
     */
    bool begin(uint8_t csPin);

    /**
     * @brief Check if SD card is available.
     */
    bool isAvailable() const;

    /**
     * @brief Log continuous flow data to flow_log.csv.
     *
     * CSV: Timestamp,Frequency_Hz,Flow_LPM,Total_Liters
     */
    bool logFlowData(const char* timestamp,
                     float frequencyHz,
                     float flowLPM,
                     float totalLiters);

    /**
     * @brief Log a completed delivery to delivery_summary.csv.
     *
     * CSV: Delivery_ID,Start_Time,End_Time,Duration_seconds,Total_Liters
     */
    bool logDelivery(const DeliveryRecord& record);

    /**
     * @brief Buffer a delivery record for later MQTT upload.
     *        Used when MQTT is offline.
     */
    bool bufferPendingDelivery(const DeliveryRecord& record);

    /**
     * @brief Read and remove the next pending delivery.
     * @param record Output — the delivery record.
     * @return true if a record was available.
     */
    bool readPendingDelivery(DeliveryRecord& record);

    /**
     * @brief Get count of pending delivery records.
     */
    size_t getPendingDeliveryCount() const;

    /**
     * @brief Clear all pending deliveries.
     */
    void clearPendingDeliveries();

    /**
     * @brief SD card mutex lock/unlock for thread safety.
     */
    static void lockSD();
    static void unlockSD();

private:

    uint8_t _csPin;
    bool    _sdReady;
    size_t  _pendingCount;
    size_t  _pendingReadOffset;

    unsigned long _lastRemountAttempt;

    /**
     * @brief Create a CSV file with header if it doesn't exist.
     */
    void initCSVFile(const char* filepath, const char* header);

    /**
     * @brief Attempt to remount SD card (with backoff).
     * @return true if remount succeeded.
     */
    bool tryRemount();

    /**
     * @brief Load pending delivery count and read offset.
     */
    void loadPendingState();

    /**
     * @brief Save pending state to a tracking file.
     */
    void savePendingState();
};

#endif // SD_LOGGER_H
