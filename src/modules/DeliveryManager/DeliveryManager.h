#ifndef DELIVERY_MANAGER_H
#define DELIVERY_MANAGER_H

#include <Arduino.h>
#include "../../utils/DataTypes.h"

// Forward declaration to avoid circular includes
class RTCManager;

/**
 * ============================================================
 * DeliveryManager
 * ------------------------------------------------------------
 * State machine for detecting diesel delivery start/end and
 * accumulating delivered volume via trapezoidal integration.
 *
 * States:
 *   IDLE       → No delivery in progress
 *   DELIVERING → Active delivery, integrating volume
 *   COMPLETED  → Delivery just finished (transient)
 *
 * Transition rules:
 *   IDLE → DELIVERING:  flowRate > DELIVERY_START_THRESHOLD_LPM
 *   DELIVERING → IDLE:  flowRate < DELIVERY_END_THRESHOLD_LPM
 *                        for DELIVERY_END_DELAY_SECONDS
 *                        continuously
 *
 * On completion, a DeliveryRecord is generated and made
 * available via getLastDelivery() / hasNewDelivery().
 * ============================================================
 */

class DeliveryManager
{
public:

    DeliveryManager();

    /**
     * @brief Initialize with reference to RTCManager for timestamps.
     *        Loads persisted delivery ID from flash.
     */
    void begin(RTCManager& rtc);

    /**
     * @brief Update state machine with latest sensor readings.
     *        Call every SENSOR_READ_INTERVAL_MS (100ms).
     *
     * @param flowRateLPM Flow rate in liters per minute.
     */
    void update(float flowRateLPM);

    /**
     * @brief Get current delivery state.
     */
    DeliveryState getState() const;

    /**
     * @brief Get total liters in the current delivery.
     *        Returns 0 if not delivering.
     */
    float getCurrentDeliveryLiters() const;

    /**
     * @brief Get the current delivery duration in seconds.
     *        Returns 0 if not delivering.
     */
    uint32_t getCurrentDeliveryDuration() const;

    /**
     * @brief Get the last completed delivery record.
     */
    const DeliveryRecord& getLastDelivery() const;

    /**
     * @brief Check if a new delivery has just completed.
     *        Returns true once per delivery, then clears.
     */
    bool hasNewDelivery();

    /**
     * @brief Get total number of deliveries recorded.
     */
    uint32_t getDeliveryCount() const;

private:

    RTCManager* _rtc;

    // State machine
    DeliveryState _state;

    // Current delivery tracking
    float    _deliveryLiters;       // Accumulated volume
    float    _lastFlowRate;         // Previous flow rate for trapezoidal integration
    unsigned long _deliveryStartMs; // millis() at delivery start
    char     _startTimestamp[24];   // Start time from RTC

    // End detection
    unsigned long _belowThresholdSince;  // millis() when current first dropped
    bool          _belowThresholdActive; // Currently counting down

    // Delivery records
    DeliveryRecord _lastRecord;
    bool           _newDeliveryFlag;
    uint32_t       _deliveryCount;

    // Previous update timestamp for integration
    unsigned long _lastUpdateMs;

    /**
     * @brief Start a new delivery.
     */
    void startDelivery();

    /**
     * @brief Complete the current delivery and generate record.
     */
    void completeDelivery();

    /**
     * @brief Integrate volume using trapezoidal rule.
     */
    void integrateVolume(float flowRateLPM, unsigned long dtMs);

    /**
     * @brief Load delivery ID counter from flash (Preferences).
     */
    void loadDeliveryId();

    /**
     * @brief Save delivery ID counter to flash.
     */
    void saveDeliveryId();
};

#endif // DELIVERY_MANAGER_H
