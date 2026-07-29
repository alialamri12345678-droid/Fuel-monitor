#ifndef FLOW_METER_H
#define FLOW_METER_H

#include <Arduino.h>

/**
 * ============================================================
 * FlowMeter
 * ------------------------------------------------------------
 * Reads a turbine flow meter via its frequency/pulse output
 * (Fout) using hardware interrupts on the ESP32.
 *
 * Signal path:
 *   Flow Meter Fout (open-collector) → 10kΩ pull-up to 3.3V → GPIO
 *   F- → ESP32 GND (common ground)
 *
 * Measurement method:
 *   - FALLING edge interrupt counts pulses with µs debounce
 *   - Frequency computed over a configurable rolling window
 *   - EMA filter for smoothing
 *   - Frequency → m³/h → L/min conversion
 *
 * Conversion:
 *   flow (m³/h) = (pulses_per_second × 3600) / meter_factor
 *   flow (L/min) = flow (m³/h) × 1000 / 60
 *
 * Legacy:
 *   The previous 4–20mA ADC-based implementation is preserved
 *   in FlowMeter.cpp.legacy for reference.
 * ============================================================
 */

class FlowMeter
{
public:

    FlowMeter();

    /**
     * @brief Initialize GPIO interrupt for pulse counting.
     * @return true if initialization succeeded.
     */
    bool begin();

    /**
     * @brief Compute frequency from accumulated pulses and
     *        convert to flow rate. Call every SENSOR_READ_INTERVAL_MS.
     *
     * Frequency is computed once per PULSE_WINDOW_MS (1s default).
     * Between windows, the last computed values are returned.
     */
    void update();

    /**
     * @brief Filtered pulse frequency in Hz.
     */
    float getFrequency() const;

    /**
     * @brief Calculated flow rate in liters per minute.
     */
    float getFlowRate() const;

    /**
     * @brief Raw pulse count in the current/last measurement window.
     */
    uint32_t getPulseCount() const;

    /**
     * @brief True if sensor readings are within valid range.
     */
    bool isValid() const;

private:

    // Latest readings
    float    _frequency;         // Filtered frequency (Hz)
    float    _frequency_raw;     // Unfiltered frequency (Hz)
    float    _flowRate;          // Flow rate (L/min)
    uint32_t _windowPulses;      // Pulses in last completed window
    bool     _sensorValid;

    // EMA filter state
    bool     _filterInitialized;

    // Measurement window tracking
    unsigned long _windowStartMs;

    /**
     * @brief Convert frequency (Hz) to flow rate (L/min).
     *
     * flow (m³/h) = (freq × 3600) / METER_FACTOR
     * flow (L/min) = flow_m3h × 1000 / 60
     */
    float frequencyToFlowRate(float freqHz) const;

    /**
     * @brief Apply EMA filter to raw frequency.
     */
    float applyFilter(float rawValue);

    /**
     * @brief Validate that frequency is within expected range.
     */
    bool validateSensor(float freqHz) const;
};

#endif // FLOW_METER_H
