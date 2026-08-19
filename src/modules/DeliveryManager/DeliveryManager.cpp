#include "DeliveryManager.h"
#include "../../config/Config.h"
#include "../../utils/Logger.h"

#include <Preferences.h>
#include <time.h>

static const char* TAG = "Delivery";

// Preferences namespace for persisting delivery counter
static const char* PREFS_NAMESPACE = "delivery";
static const char* PREFS_KEY_ID   = "next_id";

DeliveryManager::DeliveryManager()
    : _state(DeliveryState::IDLE),
      _deliveryLiters(0.0f),
      _lastFlowRate(0.0f),
      _deliveryStartMs(0),
      _belowThresholdSince(0),
      _belowThresholdActive(false),
      _newDeliveryFlag(false),
      _deliveryCount(0),
      _lastUpdateMs(0)
{
}

void DeliveryManager::begin()
{
    _state = DeliveryState::IDLE;
    _lastUpdateMs = millis();

    // Load persisted delivery counter from NVS
    loadDeliveryId();

    LOG_INFO(TAG, "Initialized. Next delivery ID: %lu",
             (unsigned long)_deliveryCount);
}

void DeliveryManager::update(float flowRateLPM)
{
    unsigned long now = millis();
    unsigned long dtMs = now - _lastUpdateMs;
    _lastUpdateMs = now;

    // Clamp dt to avoid spikes from overflow or long delays
    if (dtMs > 1000)
    {
        dtMs = 1000;
    }

    switch (_state)
    {
        // ====================================================
        //  IDLE — Waiting for delivery to start
        // ====================================================
        case DeliveryState::IDLE:
        {
            if (flowRateLPM > DELIVERY_START_THRESHOLD_LPM)
            {
                startDelivery();
            }
            break;
        }

        // ====================================================
        //  DELIVERING — Active delivery, integrating volume
        // ====================================================
        case DeliveryState::DELIVERING:
        {
            // Integrate volume regardless of threshold
            integrateVolume(flowRateLPM, dtMs);

            // Check if flow is below end threshold
            if (flowRateLPM < DELIVERY_END_THRESHOLD_LPM)
            {
                if (!_belowThresholdActive)
                {
                    // Start the countdown
                    _belowThresholdActive = true;
                    _belowThresholdSince = now;

                    LOG_DEBUG(TAG, "Flow below threshold — "
                              "starting end countdown");
                }
                else
                {
                    // Check if countdown has elapsed
                    unsigned long belowDuration =
                        now - _belowThresholdSince;

                    if (belowDuration >=
                        (DELIVERY_END_DELAY_SECONDS * 1000UL))
                    {
                        completeDelivery();
                    }
                }
            }
            else
            {
                // Flow is above threshold — reset countdown
                if (_belowThresholdActive)
                {
                    LOG_DEBUG(TAG, "Flow restored above threshold — "
                              "canceling end countdown");
                    _belowThresholdActive = false;
                }
            }

            break;
        }

        // ====================================================
        //  COMPLETED — transient, immediately returns to IDLE
        // ====================================================
        case DeliveryState::COMPLETED:
        {
            _state = DeliveryState::IDLE;
            break;
        }
    }

    _lastFlowRate = flowRateLPM;
}

// ============================================================
//  Getters
// ============================================================

DeliveryState DeliveryManager::getState() const
{
    return _state;
}

float DeliveryManager::getCurrentDeliveryLiters() const
{
    if (_state == DeliveryState::DELIVERING)
    {
        return _deliveryLiters;
    }
    return 0.0f;
}

uint32_t DeliveryManager::getCurrentDeliveryDuration() const
{
    if (_state == DeliveryState::DELIVERING)
    {
        return (millis() - _deliveryStartMs) / 1000;
    }
    return 0;
}

const DeliveryRecord& DeliveryManager::getLastDelivery() const
{
    return _lastRecord;
}

bool DeliveryManager::hasNewDelivery()
{
    if (_newDeliveryFlag)
    {
        _newDeliveryFlag = false;
        return true;
    }
    return false;
}

uint32_t DeliveryManager::getDeliveryCount() const
{
    return _deliveryCount;
}

void DeliveryManager::resetDeliveryCount()
{
    _deliveryCount = 0;
    saveDeliveryId();
    LOG_INFO(TAG, "Delivery count reset to 0");
}

// ============================================================
//  Private — State Transitions
// ============================================================

void DeliveryManager::startDelivery()
{
    _state = DeliveryState::DELIVERING;
    _deliveryLiters = 0.0f;
    _lastFlowRate = 0.0f;
    _deliveryStartMs = millis();
    _belowThresholdActive = false;

    // Use NTP time if available, otherwise fall back to uptime
    time_t now = time(nullptr);
    struct tm* timeInfo = localtime(&now);

    if (timeInfo && timeInfo->tm_year >= (2024 - 1900))
    {
        strftime(_lastRecord.startTime, sizeof(_lastRecord.startTime),
                 "%Y-%m-%d %H:%M:%S", timeInfo);
    }
    else
    {
        unsigned long uptimeSec = millis() / 1000;
        snprintf(_lastRecord.startTime, sizeof(_lastRecord.startTime),
                 "uptime:%lu", uptimeSec);
    }

    LOG_INFO(TAG, "=== DELIVERY STARTED ===");
    LOG_INFO(TAG, "Start time: %s", _lastRecord.startTime);
}

void DeliveryManager::completeDelivery()
{
    // Calculate duration
    uint32_t durationMs = millis() - _deliveryStartMs;
    uint32_t durationSec = durationMs / 1000;

    // Increment and persist delivery counter
    _deliveryCount++;
    saveDeliveryId();

    // Build delivery record
    _lastRecord.deliveryId = _deliveryCount;

    // Use NTP time if available, otherwise fall back to uptime
    time_t now = time(nullptr);
    struct tm* timeInfo = localtime(&now);

    if (timeInfo && timeInfo->tm_year >= (2024 - 1900))
    {
        strftime(_lastRecord.endTime, sizeof(_lastRecord.endTime),
                 "%Y-%m-%d %H:%M:%S", timeInfo);
    }
    else
    {
        unsigned long uptimeSec = millis() / 1000;
        snprintf(_lastRecord.endTime, sizeof(_lastRecord.endTime),
                 "uptime:%lu", uptimeSec);
    }

    _lastRecord.durationSeconds = durationSec;
    _lastRecord.totalLiters = _deliveryLiters;

    _newDeliveryFlag = true;

    LOG_INFO(TAG, "=== DELIVERY COMPLETED ===");
    LOG_INFO(TAG, "  ID:        %lu", (unsigned long)_lastRecord.deliveryId);
    LOG_INFO(TAG, "  Start:     %s", _lastRecord.startTime);
    LOG_INFO(TAG, "  End:       %s", _lastRecord.endTime);
    LOG_INFO(TAG, "  Duration:  %lu seconds", (unsigned long)durationSec);
    LOG_INFO(TAG, "  Volume:    %.2f liters", _lastRecord.totalLiters);

    // Transition to COMPLETED (will auto-return to IDLE on next update)
    _state = DeliveryState::COMPLETED;
    _belowThresholdActive = false;
}

void DeliveryManager::integrateVolume(float flowRateLPM,
                                       unsigned long dtMs)
{
    if (dtMs == 0) return;

    // Trapezoidal integration:
    //   avgFlow = (current + previous) / 2
    //   volume  = avgFlow * dt_minutes
    float avgFlow = (flowRateLPM + _lastFlowRate) / 2.0f;
    float dtMinutes = static_cast<float>(dtMs) / 60000.0f;
    float volumeIncrement = avgFlow * dtMinutes;

    if (volumeIncrement > 0.0f)
    {
        _deliveryLiters += volumeIncrement;
    }
}

// ============================================================
//  Private — Persistent Storage
// ============================================================

void DeliveryManager::loadDeliveryId()
{
    Preferences prefs;
    prefs.begin(PREFS_NAMESPACE, true);  // Read-only
    _deliveryCount = prefs.getUInt(PREFS_KEY_ID, 0);
    prefs.end();

    LOG_DEBUG(TAG, "Loaded delivery ID from NVS: %lu",
              (unsigned long)_deliveryCount);
}

void DeliveryManager::saveDeliveryId()
{
    Preferences prefs;
    prefs.begin(PREFS_NAMESPACE, false);  // Read-write
    prefs.putUInt(PREFS_KEY_ID, _deliveryCount);
    prefs.end();

    LOG_DEBUG(TAG, "Saved delivery ID to NVS: %lu",
              (unsigned long)_deliveryCount);
}
