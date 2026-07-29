#ifndef RTC_MANAGER_H
#define RTC_MANAGER_H

#include <Arduino.h>
#include <RTClib.h>

/**
 * ============================================================
 * RTCManager
 * ------------------------------------------------------------
 * Manages the DS3231 Real-Time Clock over I2C.
 *
 * Features:
 *  - Initialize and verify DS3231 communication
 *  - Formatted timestamp retrieval
 *  - Lost-power detection and recovery
 *  - NTP-based time synchronization support
 *  - Fallback to compile time or epoch on failure
 * ============================================================
 */

class RTCManager
{
public:

    RTCManager();

    /**
     * @brief Initialize DS3231 RTC over I2C.
     * @return true if RTC is communicating.
     */
    bool begin();

    /**
     * @brief Check if RTC is available and responding.
     */
    bool isAvailable() const;

    /**
     * @brief Get formatted timestamp string.
     * @param buffer Output buffer (minimum 24 chars).
     * @param len Buffer length.
     *
     * Format: "YYYY-MM-DD HH:MM:SS"
     */
    void getTimestamp(char* buffer, size_t len);

    /**
     * @brief Get current DateTime from RTC.
     */
    DateTime getDateTime();

    /**
     * @brief Set RTC time (e.g., from NTP synchronization).
     */
    void adjustTime(const DateTime& dt);

    /**
     * @brief Get RTC temperature (DS3231 has built-in temp sensor).
     * @return Temperature in degrees Celsius.
     */
    float getTemperature();

private:

    RTC_DS3231 _rtc;
    bool       _rtcReady;
};

#endif // RTC_MANAGER_H
