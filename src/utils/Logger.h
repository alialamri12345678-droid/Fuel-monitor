#ifndef LOGGER_H
#define LOGGER_H

#include <Arduino.h>
#include <stdarg.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

/**
 * ============================================================
 * Logger
 * ------------------------------------------------------------
 * Professional logging utility for ESP32 applications.
 *
 * Features:
 *  - Log Levels (ERROR, WARNING, INFO, DEBUG, VERBOSE)
 *  - Timestamp (millis since boot)
 *  - Module Name tagging
 *  - printf-style formatting
 *  - Compile-time level filtering
 *  - Thread-safe serial output via FreeRTOS mutex
 * ============================================================
 */

class Logger
{
public:

    enum class Level : uint8_t
    {
        NONE = 0,
        ERROR,
        WARNING,
        INFO,
        DEBUG,
        VERBOSE
    };

    /**
     * Initialize Logger with baud rate and default level.
     */
    static void begin(uint32_t baudRate = 115200,
                      Level level = Level::INFO);

    /**
     * Change current log level at runtime.
     */
    static void setLevel(Level level);

    /**
     * Get current log level.
     */
    static Level getLevel();

    /**
     * Enable/Disable timestamp prefix.
     */
    static void enableTimestamp(bool enable);

    /**
     * Enable/Disable module name prefix.
     */
    static void enableModule(bool enable);

    /**
     * Enable/Disable file & line information.
     */
    static void enableSourceInfo(bool enable);

    /**
     * Print free heap memory.
     */
    static void printHeap();

    /**
     * Logging functions — called via macros.
     */
    static void error(const char* module,
                      const char* file,
                      int line,
                      const char* format,
                      ...);

    static void warning(const char* module,
                        const char* file,
                        int line,
                        const char* format,
                        ...);

    static void info(const char* module,
                     const char* file,
                     int line,
                     const char* format,
                     ...);

    static void debug(const char* module,
                      const char* file,
                      int line,
                      const char* format,
                      ...);

    static void verbose(const char* module,
                        const char* file,
                        int line,
                        const char* format,
                        ...);

private:

    static void log(Level level,
                    const char* module,
                    const char* file,
                    int line,
                    const char* format,
                    va_list args);

    static const char* levelToString(Level level);

    static bool initialized;
    static bool timestampEnabled;
    static bool moduleEnabled;
    static bool sourceInfoEnabled;

    static Level currentLevel;
    static unsigned long startTime;

    // Mutex protecting Serial output from concurrent task logging
    static SemaphoreHandle_t _serialMutex;
};

/*==============================================================
    Logging Macros
==============================================================*/

#define LOG_ERROR(module, ...)   Logger::error(module, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_WARNING(module, ...) Logger::warning(module, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_INFO(module, ...)    Logger::info(module, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_DEBUG(module, ...)   Logger::debug(module, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_VERBOSE(module, ...) Logger::verbose(module, __FILE__, __LINE__, __VA_ARGS__)

#endif // LOGGER_H
