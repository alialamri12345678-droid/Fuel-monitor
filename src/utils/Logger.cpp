#include "Logger.h"

bool Logger::initialized = false;
bool Logger::timestampEnabled = true;
bool Logger::moduleEnabled = true;
bool Logger::sourceInfoEnabled = false;

Logger::Level Logger::currentLevel = Logger::Level::INFO;

unsigned long Logger::startTime = 0;

SemaphoreHandle_t Logger::_serialMutex = NULL;

void Logger::begin(uint32_t baudRate, Level level)
{
    Serial.begin(baudRate);

    _serialMutex = xSemaphoreCreateMutex();

    currentLevel = level;
    initialized = true;
    startTime = millis();

    Serial.println();
    Serial.println("==========================================");
    Serial.println("  Diesel Delivery Monitor — Logger Ready");
    Serial.println("==========================================");
}

void Logger::setLevel(Level level)
{
    currentLevel = level;
}

Logger::Level Logger::getLevel()
{
    return currentLevel;
}

void Logger::enableTimestamp(bool enable)
{
    timestampEnabled = enable;
}

void Logger::enableModule(bool enable)
{
    moduleEnabled = enable;
}

void Logger::enableSourceInfo(bool enable)
{
    sourceInfoEnabled = enable;
}

void Logger::printHeap()
{
#if defined(ESP32)
    LOG_INFO("SYSTEM", "Free Heap: %u bytes", ESP.getFreeHeap());
#endif
}

void Logger::error(const char* module,
                   const char* file,
                   int line,
                   const char* format,
                   ...)
{
    va_list args;
    va_start(args, format);
    log(Level::ERROR, module, file, line, format, args);
    va_end(args);
}

void Logger::warning(const char* module,
                     const char* file,
                     int line,
                     const char* format,
                     ...)
{
    va_list args;
    va_start(args, format);
    log(Level::WARNING, module, file, line, format, args);
    va_end(args);
}

void Logger::info(const char* module,
                  const char* file,
                  int line,
                  const char* format,
                  ...)
{
    va_list args;
    va_start(args, format);
    log(Level::INFO, module, file, line, format, args);
    va_end(args);
}

void Logger::debug(const char* module,
                   const char* file,
                   int line,
                   const char* format,
                   ...)
{
    va_list args;
    va_start(args, format);
    log(Level::DEBUG, module, file, line, format, args);
    va_end(args);
}

void Logger::verbose(const char* module,
                     const char* file,
                     int line,
                     const char* format,
                     ...)
{
    va_list args;
    va_start(args, format);
    log(Level::VERBOSE, module, file, line, format, args);
    va_end(args);
}

void Logger::log(Level level,
                 const char* module,
                 const char* file,
                 int line,
                 const char* format,
                 va_list args)
{
    if (!initialized)
        return;

    if (static_cast<uint8_t>(level) >
        static_cast<uint8_t>(currentLevel))
        return;

    char message[256];

    vsnprintf(message,
              sizeof(message),
              format,
              args);

    // Take the serial mutex (with 100ms timeout to avoid deadlock)
    bool haveMutex = (_serialMutex != NULL) &&
                     (xSemaphoreTake(_serialMutex, pdMS_TO_TICKS(100)) == pdTRUE);

    if (timestampEnabled)
    {
        Serial.print("[");
        Serial.print(millis() - startTime);
        Serial.print(" ms] ");
    }

    Serial.print("[");
    Serial.print(levelToString(level));
    Serial.print("] ");

    if (moduleEnabled)
    {
        Serial.print("[");
        Serial.print(module);
        Serial.print("] ");
    }

    if (sourceInfoEnabled)
    {
        Serial.print("[");
        Serial.print(file);
        Serial.print(":");
        Serial.print(line);
        Serial.print("] ");
    }

    Serial.println(message);

    if (haveMutex)
    {
        xSemaphoreGive(_serialMutex);
    }
}

const char* Logger::levelToString(Level level)
{
    switch (level)
    {
        case Level::ERROR:
            return "ERROR";

        case Level::WARNING:
            return "WARNING";

        case Level::INFO:
            return "INFO";

        case Level::DEBUG:
            return "DEBUG";

        case Level::VERBOSE:
            return "VERBOSE";

        default:
            return "NONE";
    }
}
