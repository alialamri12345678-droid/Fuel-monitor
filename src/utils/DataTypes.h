#ifndef DATA_TYPES_H
#define DATA_TYPES_H

#include <Arduino.h>

/**
 * ============================================================
 * DataTypes.h
 * ------------------------------------------------------------
 * Shared data structures for the Diesel Delivery Verification
 * & Monitoring System.
 *
 * These types are used across multiple modules to pass data
 * without creating inter-module dependencies.
 * ============================================================
 */

// ============================================================
//  Delivery State Machine
// ============================================================

enum class DeliveryState : uint8_t
{
    IDLE = 0,       // No delivery in progress
    DELIVERING,     // Active delivery — flow detected
    COMPLETED       // Delivery just finished (transient state)
};

/**
 * @brief Convert DeliveryState to human-readable string.
 */
inline const char* deliveryStateToString(DeliveryState state)
{
    switch (state)
    {
        case DeliveryState::IDLE:       return "IDLE";
        case DeliveryState::DELIVERING: return "RUNNING";
        case DeliveryState::COMPLETED:  return "COMPLETED";
        default:                        return "UNKNOWN";
    }
}

// ============================================================
//  Flow Measurement Data
// ============================================================

struct FlowData
{
    float    frequencyHz;        // Measured pulse frequency (Hz)
    float    flowRateLPM;        // Flow rate (liters per minute)
    float    totalLiters;        // Accumulated delivered volume (liters)
    uint32_t pulseCount;         // Total accumulated pulses in window
    bool     sensorValid;        // True if reading is within valid range

    FlowData()
        : frequencyHz(0.0f),
          flowRateLPM(0.0f),
          totalLiters(0.0f),
          pulseCount(0),
          sensorValid(false)
    {
    }
};

// ============================================================
//  Completed Delivery Record
// ============================================================

struct DeliveryRecord
{
    uint32_t deliveryId;            // Unique auto-incrementing ID
    char     startTime[24];         // "YYYY-MM-DD HH:MM:SS"
    char     endTime[24];           // "YYYY-MM-DD HH:MM:SS"
    uint32_t durationSeconds;       // Total duration in seconds
    float    totalLiters;           // Total delivered volume

    DeliveryRecord()
        : deliveryId(0),
          durationSeconds(0),
          totalLiters(0.0f)
    {
        startTime[0] = '\0';
        endTime[0]   = '\0';
    }
};

#endif // DATA_TYPES_H
