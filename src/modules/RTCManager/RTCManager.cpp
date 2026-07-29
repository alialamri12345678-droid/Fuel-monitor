#include "RTCManager.h"
#include "../../config/Config.h"
#include "../../utils/Logger.h"
#include "../../utils/ErrorHandler.h"

#include <Wire.h>

static const char* TAG = "RTC";

RTCManager::RTCManager()
    : _rtcReady(false)
{
}

bool RTCManager::begin()
{
    LOG_INFO(TAG, "Initializing DS3231 RTC (SDA=%d, SCL=%d)...",
             I2C_SDA_PIN, I2C_SCL_PIN);

    // Initialize I2C with configured pins
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);

    if (!_rtc.begin())
    {
        LOG_ERROR(TAG, "DS3231 initialization failed! Check wiring.");
        _rtcReady = false;
        ErrorHandler::setError(SystemError::ERROR_RTC_FAILURE);
        return false;
    }

    _rtcReady = true;

    // Check if RTC lost power (battery dead or first boot)
    if (_rtc.lostPower())
    {
        LOG_WARNING(TAG, "RTC lost power — setting to compile time");
        _rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }

    // Read and display current time
    DateTime now = _rtc.now();
    LOG_INFO(TAG, "RTC OK. Time: %04u-%02u-%02u %02u:%02u:%02u",
             now.year(), now.month(), now.day(),
             now.hour(), now.minute(), now.second());

    // Validate year is reasonable
    if (now.year() < 2024 || now.year() > 2099)
    {
        LOG_WARNING(TAG, "RTC year (%u) looks invalid — setting to compile time",
                    now.year());
        _rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }

    ErrorHandler::clearError(SystemError::ERROR_RTC_FAILURE);
    return true;
}

bool RTCManager::isAvailable() const
{
    return _rtcReady;
}

void RTCManager::getTimestamp(char* buffer, size_t len)
{
    if (buffer == nullptr || len == 0) return;

    // 1. Try hardware RTC if available
    if (_rtcReady)
    {
        DateTime now = _rtc.now();
        if (now.year() >= 2024)
        {
            snprintf(buffer, len, "%04u-%02u-%02u %02u:%02u:%02u",
                     now.year(), now.month(), now.day(),
                     now.hour(), now.minute(), now.second());
            return;
        }
    }

    // 2. Fallback: ESP32 system time (NTP synced)
    struct tm timeinfo;
    if (getLocalTime(&timeinfo, 10))
    {
        if (timeinfo.tm_year + 1900 >= 2024)
        {
            snprintf(buffer, len, "%04d-%02d-%02d %02d:%02d:%02d",
                     timeinfo.tm_year + 1900, timeinfo.tm_mon + 1,
                     timeinfo.tm_mday,
                     timeinfo.tm_hour, timeinfo.tm_min,
                     timeinfo.tm_sec);
            return;
        }
    }

    // 3. Last resort: RTC raw reading or epoch fallback
    if (_rtcReady)
    {
        DateTime now = _rtc.now();
        snprintf(buffer, len, "%04u-%02u-%02u %02u:%02u:%02u",
                 now.year(), now.month(), now.day(),
                 now.hour(), now.minute(), now.second());
    }
    else
    {
        snprintf(buffer, len, "1970-01-01 00:00:00");
    }
}

DateTime RTCManager::getDateTime()
{
    if (_rtcReady)
    {
        return _rtc.now();
    }

    // Return epoch if RTC unavailable
    return DateTime(1970, 1, 1, 0, 0, 0);
}

void RTCManager::adjustTime(const DateTime& dt)
{
    if (!_rtcReady)
    {
        // Try to re-initialize
        _rtcReady = _rtc.begin();
    }

    if (_rtcReady)
    {
        _rtc.adjust(dt);
        LOG_INFO(TAG, "RTC time adjusted to %04u-%02u-%02u %02u:%02u:%02u",
                 dt.year(), dt.month(), dt.day(),
                 dt.hour(), dt.minute(), dt.second());
    }
    else
    {
        LOG_ERROR(TAG, "Cannot adjust time — RTC not available");
    }
}

float RTCManager::getTemperature()
{
    if (_rtcReady)
    {
        return _rtc.getTemperature();
    }
    return 0.0f;
}
