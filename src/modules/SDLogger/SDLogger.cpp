#include "SDLogger.h"
#include "../../config/Config.h"
#include "../../utils/Logger.h"
#include "../../utils/ErrorHandler.h"

#include <SPI.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

static const char* TAG = "SDLogger";
static SemaphoreHandle_t _sdMutex = NULL;

// ============================================================
//  Thread-safe SD access
// ============================================================

void SDLogger::lockSD()
{
    if (_sdMutex != NULL)
    {
        xSemaphoreTake(_sdMutex, portMAX_DELAY);
    }
}

void SDLogger::unlockSD()
{
    if (_sdMutex != NULL)
    {
        xSemaphoreGive(_sdMutex);
    }
}

// ============================================================
//  Constructor
// ============================================================

SDLogger::SDLogger()
    : _csPin(SD_CS_PIN),
      _sdReady(false),
      _pendingCount(0),
      _pendingReadOffset(0),
      _lastRemountAttempt(0)
{
    if (_sdMutex == NULL)
    {
        _sdMutex = xSemaphoreCreateMutex();
    }
}

// ============================================================
//  Initialization
// ============================================================

bool SDLogger::begin(uint8_t csPin)
{
    _csPin = csPin;

    LOG_INFO(TAG, "Initializing SD card (CS Pin %u)...", _csPin);

    lockSD();

    if (!SD.begin(_csPin))
    {
        LOG_ERROR(TAG, "SD card initialization failed!");
        _sdReady = false;
        ErrorHandler::setError(SystemError::ERROR_SD_FAILURE);
        unlockSD();
        return false;
    }

    _sdReady = true;

    // Report card info
    uint8_t cardType = SD.cardType();
    if (cardType == CARD_NONE)
    {
        LOG_ERROR(TAG, "No SD card detected!");
        _sdReady = false;
        ErrorHandler::setError(SystemError::ERROR_SD_FAILURE);
        unlockSD();
        return false;
    }

    const char* typeStr = "UNKNOWN";
    if (cardType == CARD_MMC)       typeStr = "MMC";
    else if (cardType == CARD_SD)   typeStr = "SDSC";
    else if (cardType == CARD_SDHC) typeStr = "SDHC";

    LOG_INFO(TAG, "SD card mounted: %s, Size: %llu MB",
             typeStr, SD.cardSize() / (1024 * 1024));

    // Create CSV files with headers
    initCSVFile(FLOW_LOG_FILE,
                "Timestamp,Frequency_Hz,Flow_LPM,Total_Liters");

    initCSVFile(DELIVERY_SUMMARY_FILE,
                "Delivery_ID,Start_Time,End_Time,Duration_seconds,Total_Liters");

    unlockSD();

    // Load pending delivery state
    loadPendingState();

    ErrorHandler::clearError(SystemError::ERROR_SD_FAILURE);

    LOG_INFO(TAG, "SD card initialized successfully");
    return true;
}

bool SDLogger::isAvailable() const
{
    return (_sdReady && SD.cardType() != CARD_NONE);
}

// ============================================================
//  Flow Data Logging
// ============================================================

bool SDLogger::logFlowData(const char* timestamp,
                           float frequencyHz,
                           float flowLPM,
                           float totalLiters)
{
    if (!_sdReady && !tryRemount())
    {
        return false;
    }

    lockSD();

    File file = SD.open(FLOW_LOG_FILE, FILE_APPEND);
    if (!file)
    {
        LOG_ERROR(TAG, "Failed to open %s for writing",
                  FLOW_LOG_FILE);
        unlockSD();
        return false;
    }

    // CSV: Timestamp,Frequency_Hz,Flow_LPM,Total_Liters
    char row[128];
    snprintf(row, sizeof(row), "%s,%.2f,%.2f,%.2f\n",
             timestamp, frequencyHz, flowLPM, totalLiters);

    file.print(row);
    file.flush();
    file.close();

    unlockSD();
    return true;
}

// ============================================================
//  Delivery Summary Logging
// ============================================================

bool SDLogger::logDelivery(const DeliveryRecord& record)
{
    if (!_sdReady && !tryRemount())
    {
        return false;
    }

    lockSD();

    File file = SD.open(DELIVERY_SUMMARY_FILE, FILE_APPEND);
    if (!file)
    {
        LOG_ERROR(TAG, "Failed to open %s for writing",
                  DELIVERY_SUMMARY_FILE);
        unlockSD();
        return false;
    }

    // CSV: Delivery_ID,Start_Time,End_Time,Duration_seconds,Total_Liters
    char row[192];
    snprintf(row, sizeof(row), "%lu,%s,%s,%lu,%.2f\n",
             (unsigned long)record.deliveryId,
             record.startTime,
             record.endTime,
             (unsigned long)record.durationSeconds,
             record.totalLiters);

    file.print(row);
    file.flush();
    file.close();

    unlockSD();

    LOG_INFO(TAG, "Delivery #%lu logged to SD",
             (unsigned long)record.deliveryId);
    return true;
}

// ============================================================
//  Pending Delivery Buffer (for offline MQTT sync)
// ============================================================

bool SDLogger::bufferPendingDelivery(const DeliveryRecord& record)
{
    if (!_sdReady) return false;

    if (_pendingCount >= MAX_PENDING_DELIVERIES)
    {
        LOG_WARNING(TAG, "Pending buffer full (%d), dropping delivery",
                    MAX_PENDING_DELIVERIES);
        return false;
    }

    lockSD();

    File file = SD.open(PENDING_DELIVERY_FILE, FILE_APPEND);
    if (!file)
    {
        LOG_ERROR(TAG, "Failed to open pending deliveries file");
        unlockSD();
        return false;
    }

    size_t written = file.write(
        reinterpret_cast<const uint8_t*>(&record),
        sizeof(DeliveryRecord));

    file.flush();
    file.close();
    unlockSD();

    if (written == sizeof(DeliveryRecord))
    {
        _pendingCount++;
        savePendingState();
        LOG_INFO(TAG, "Buffered pending delivery #%lu (%d total)",
                 (unsigned long)record.deliveryId, _pendingCount);
        return true;
    }

    LOG_ERROR(TAG, "Failed to write pending delivery");
    return false;
}

bool SDLogger::readPendingDelivery(DeliveryRecord& record)
{
    if (_pendingCount == 0 || _pendingReadOffset >= _pendingCount)
    {
        return false;
    }

    lockSD();

    File file = SD.open(PENDING_DELIVERY_FILE, FILE_READ);
    if (!file)
    {
        unlockSD();
        return false;
    }

    size_t seekPos = _pendingReadOffset * sizeof(DeliveryRecord);
    if (!file.seek(seekPos))
    {
        file.close();
        unlockSD();
        return false;
    }

    size_t bytesRead = file.read(
        reinterpret_cast<uint8_t*>(&record),
        sizeof(DeliveryRecord));

    file.close();
    unlockSD();

    if (bytesRead == sizeof(DeliveryRecord))
    {
        _pendingReadOffset++;
        savePendingState();

        // If all records have been read, clean up
        if (_pendingReadOffset >= _pendingCount)
        {
            clearPendingDeliveries();
        }

        return true;
    }

    return false;
}

size_t SDLogger::getPendingDeliveryCount() const
{
    if (_pendingCount > _pendingReadOffset)
    {
        return _pendingCount - _pendingReadOffset;
    }
    return 0;
}

void SDLogger::clearPendingDeliveries()
{
    lockSD();
    SD.remove(PENDING_DELIVERY_FILE);
    SD.remove("/pending_state.bin");
    unlockSD();

    _pendingCount = 0;
    _pendingReadOffset = 0;

    LOG_INFO(TAG, "Pending deliveries cleared");
}

// ============================================================
//  Private — File Initialization
// ============================================================

void SDLogger::initCSVFile(const char* filepath, const char* header)
{
    if (!_sdReady) return;

    if (!SD.exists(filepath))
    {
        File file = SD.open(filepath, FILE_WRITE);
        if (file)
        {
            file.println(header);
            file.flush();
            file.close();
            LOG_INFO(TAG, "Created: %s", filepath);
        }
        else
        {
            LOG_ERROR(TAG, "Failed to create: %s", filepath);
        }
    }
    else
    {
        LOG_INFO(TAG, "Existing file: %s", filepath);
    }
}

// ============================================================
//  Private — SD Remount
// ============================================================

bool SDLogger::tryRemount()
{
    unsigned long now = millis();

    // Backoff: don't try more than once every 30 seconds
    if (now - _lastRemountAttempt < 30000)
    {
        return false;
    }

    _lastRemountAttempt = now;

    LOG_INFO(TAG, "Attempting SD card remount...");

    lockSD();

    if (SD.begin(_csPin))
    {
        _sdReady = true;

        initCSVFile(FLOW_LOG_FILE,
                    "Timestamp,Frequency_Hz,Flow_LPM,Total_Liters");
        initCSVFile(DELIVERY_SUMMARY_FILE,
                    "Delivery_ID,Start_Time,End_Time,Duration_seconds,Total_Liters");

        ErrorHandler::clearError(SystemError::ERROR_SD_FAILURE);
        LOG_INFO(TAG, "SD card remounted successfully");
    }
    else
    {
        _sdReady = false;
        ErrorHandler::setError(SystemError::ERROR_SD_FAILURE);
        LOG_ERROR(TAG, "SD card remount failed");
    }

    unlockSD();
    return _sdReady;
}

// ============================================================
//  Private — Pending State Persistence
// ============================================================

void SDLogger::loadPendingState()
{
    lockSD();

    if (SD.exists("/pending_state.bin"))
    {
        File f = SD.open("/pending_state.bin", FILE_READ);
        if (f && f.size() >= sizeof(size_t) * 2)
        {
            f.read(reinterpret_cast<uint8_t*>(&_pendingCount),
                   sizeof(size_t));
            f.read(reinterpret_cast<uint8_t*>(&_pendingReadOffset),
                   sizeof(size_t));
            f.close();

            LOG_INFO(TAG, "Loaded pending state: %d total, %d read",
                     _pendingCount, _pendingReadOffset);
        }
    }
    else
    {
        // Count from file size if state file is missing
        if (SD.exists(PENDING_DELIVERY_FILE))
        {
            File f = SD.open(PENDING_DELIVERY_FILE, FILE_READ);
            if (f)
            {
                _pendingCount = f.size() / sizeof(DeliveryRecord);
                _pendingReadOffset = 0;
                f.close();

                LOG_INFO(TAG, "Recovered %d pending deliveries from file",
                         _pendingCount);
            }
        }
    }

    unlockSD();
}

void SDLogger::savePendingState()
{
    lockSD();

    File f = SD.open("/pending_state.bin", FILE_WRITE);
    if (f)
    {
        f.write(reinterpret_cast<const uint8_t*>(&_pendingCount),
                sizeof(size_t));
        f.write(reinterpret_cast<const uint8_t*>(&_pendingReadOffset),
                sizeof(size_t));
        f.flush();
        f.close();
    }

    unlockSD();
}
