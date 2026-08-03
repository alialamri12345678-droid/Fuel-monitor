#ifndef ERROR_HANDLER_H
#define ERROR_HANDLER_H

#include <Arduino.h>

/**
 * ============================================================
 * ErrorHandler
 * ------------------------------------------------------------
 * Centralized error tracking for the Diesel Delivery
 * Monitoring System.
 *
 * Provides static methods to set, clear, and query system
 * errors. Designed for integration with logging and MQTT
 * status reporting.
 * ============================================================
 */

// ============================================================
//  Error Codes
// ============================================================

enum class SystemError : uint8_t
{
    NONE = 0,
    ERROR_WIFI_FAILURE,
    ERROR_MQTT_FAILURE,
    ERROR_SENSOR_OUT_OF_RANGE
};

// ============================================================
//  ErrorHandler Class
// ============================================================

class ErrorHandler
{
public:

    static constexpr uint8_t MAX_ERRORS = 3;

    /**
     * @brief Set an error flag.
     */
    static void setError(SystemError error)
    {
        uint8_t idx = static_cast<uint8_t>(error);
        if (idx > 0 && idx <= MAX_ERRORS)
        {
            _errors[idx - 1] = true;
        }
    }

    /**
     * @brief Clear an error flag.
     */
    static void clearError(SystemError error)
    {
        uint8_t idx = static_cast<uint8_t>(error);
        if (idx > 0 && idx <= MAX_ERRORS)
        {
            _errors[idx - 1] = false;
        }
    }

    /**
     * @brief Check if a specific error is active.
     */
    static bool hasError(SystemError error)
    {
        uint8_t idx = static_cast<uint8_t>(error);
        if (idx > 0 && idx <= MAX_ERRORS)
        {
            return _errors[idx - 1];
        }
        return false;
    }

    /**
     * @brief Check if any error is active.
     */
    static bool hasAnyError()
    {
        for (uint8_t i = 0; i < MAX_ERRORS; i++)
        {
            if (_errors[i]) return true;
        }
        return false;
    }

    /**
     * @brief Clear all errors.
     */
    static void clearAll()
    {
        for (uint8_t i = 0; i < MAX_ERRORS; i++)
        {
            _errors[i] = false;
        }
    }

    /**
     * @brief Get human-readable error string.
     */
    static const char* getErrorString(SystemError error)
    {
        switch (error)
        {
            case SystemError::NONE:
                return "NONE";
            case SystemError::ERROR_WIFI_FAILURE:
                return "WIFI_FAILURE";
            case SystemError::ERROR_MQTT_FAILURE:
                return "MQTT_FAILURE";
            case SystemError::ERROR_SENSOR_OUT_OF_RANGE:
                return "SENSOR_OUT_OF_RANGE";
            default:
                return "UNKNOWN";
        }
    }

    /**
     * @brief Build a comma-separated string of active errors.
     * @param buffer Output buffer.
     * @param len    Buffer length.
     */
    static void getActiveErrorsString(char* buffer, size_t len)
    {
        if (buffer == nullptr || len == 0) return;
        buffer[0] = '\0';

        bool first = true;
        for (uint8_t i = 0; i < MAX_ERRORS; i++)
        {
            if (_errors[i])
            {
                SystemError e = static_cast<SystemError>(i + 1);
                if (!first)
                {
                    strncat(buffer, ",", len - strlen(buffer) - 1);
                }
                strncat(buffer, getErrorString(e),
                        len - strlen(buffer) - 1);
                first = false;
            }
        }

        if (first)
        {
            strncpy(buffer, "NONE", len - 1);
            buffer[len - 1] = '\0';
        }
    }

private:

    static bool _errors[MAX_ERRORS];
};

#endif // ERROR_HANDLER_H
