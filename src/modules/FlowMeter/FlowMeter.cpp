#include "FlowMeter.h"
#include "../../config/Config.h"
#include "../../utils/Logger.h"
#include "../../utils/ErrorHandler.h"

static const char* TAG = "FlowMeter";

// ============================================================
//  ISR — Interrupt Service Routine (runs in IRAM)
// ============================================================

// Shared state between ISR and main loop (volatile + atomic)
static volatile uint32_t _isrPulseCount = 0;
static volatile unsigned long _isrLastPulseUs = 0;

/**
 * @brief FALLING edge ISR with microsecond debounce.
 *
 * Called on every falling edge of the Fout signal.
 * Rejects pulses that arrive faster than PULSE_DEBOUNCE_US
 * since the last valid pulse (contact bounce / noise rejection).
 */
static void IRAM_ATTR pulseISR()
{
    unsigned long nowUs = micros();

    // Debounce: ignore pulses arriving too fast
    if ((nowUs - _isrLastPulseUs) >= PULSE_DEBOUNCE_US)
    {
        _isrPulseCount++;
        _isrLastPulseUs = nowUs;
    }
}

// ============================================================
//  Constructor
// ============================================================

FlowMeter::FlowMeter()
    : _frequency(0.0f),
      _frequency_raw(0.0f),
      _flowRate(0.0f),
      _windowPulses(0),
      _sensorValid(true),
      _filterInitialized(false),
      _windowStartMs(0)
{
}

// ============================================================
//  Initialization
// ============================================================

bool FlowMeter::begin()
{
    LOG_INFO(TAG, "Initializing pulse input on GPIO %d", PULSE_INPUT_PIN);

    // Configure GPIO as input
    // GPIO 34 is input-only, no internal pull-up available
    // External 10kΩ pull-up to 3.3V is required
    pinMode(PULSE_INPUT_PIN, INPUT);

    // Attach interrupt on FALLING edge (open-collector goes LOW on pulse)
    attachInterrupt(
        digitalPinToInterrupt(PULSE_INPUT_PIN),
        pulseISR,
        FALLING
    );

    // Initialize window timing
    _windowStartMs = millis();

    // Reset ISR counters
    noInterrupts();
    _isrPulseCount = 0;
    _isrLastPulseUs = 0;
    interrupts();

    LOG_INFO(TAG, "Pulse input initialized successfully");
    LOG_INFO(TAG, "  Meter Factor: %.0f pulses/m3", METER_FACTOR);
    LOG_INFO(TAG, "  Measurement Window: %d ms", PULSE_WINDOW_MS);
    LOG_INFO(TAG, "  Debounce: %d us", PULSE_DEBOUNCE_US);
    LOG_INFO(TAG, "  Max Valid Freq: %.0f Hz", FREQ_MAX_VALID_HZ);

    ErrorHandler::clearError(SystemError::ERROR_PULSE_INPUT_FAILURE);
    return true;
}

// ============================================================
//  Update — Called every SENSOR_READ_INTERVAL_MS (100ms)
// ============================================================

void FlowMeter::update()
{
    unsigned long now = millis();
    unsigned long elapsed = now - _windowStartMs;

    // Only compute frequency when the measurement window completes
    if (elapsed >= PULSE_WINDOW_MS)
    {
        // 1. Atomically read and reset the ISR pulse counter
        noInterrupts();
        uint32_t pulses = _isrPulseCount;
        _isrPulseCount = 0;
        interrupts();

        _windowPulses = pulses;

        // 2. Calculate raw frequency (Hz)
        // freq = pulses / (elapsed_seconds)
        float elapsedSec = static_cast<float>(elapsed) / 1000.0f;
        _frequency_raw = static_cast<float>(pulses) / elapsedSec;

        // 3. Validate frequency range
        _sensorValid = validateSensor(_frequency_raw);

        if (_sensorValid)
        {
            // 4. Apply EMA smoothing filter
            _frequency = applyFilter(_frequency_raw);

            // 5. Convert frequency to flow rate (L/min)
            _flowRate = frequencyToFlowRate(_frequency);

            // 6. Apply minimum flow threshold (noise floor)
            if (_flowRate < FLOW_MIN_THRESHOLD_LPM)
            {
                _flowRate = 0.0f;
            }

            ErrorHandler::clearError(SystemError::ERROR_SENSOR_OUT_OF_RANGE);
        }
        else
        {
            _flowRate = 0.0f;
            ErrorHandler::setError(SystemError::ERROR_SENSOR_OUT_OF_RANGE);
        }

        // Reset window for next measurement
        _windowStartMs = now;
    }

    // Between windows: values from last completed window are retained
}

// ============================================================
//  Getters
// ============================================================

float FlowMeter::getFrequency() const
{
    return _frequency;
}

float FlowMeter::getFlowRate() const
{
    return _flowRate;
}

uint32_t FlowMeter::getPulseCount() const
{
    return _windowPulses;
}

bool FlowMeter::isValid() const
{
    return _sensorValid;
}

// ============================================================
//  Private — Conversion Functions
// ============================================================

float FlowMeter::frequencyToFlowRate(float freqHz) const
{
    // Step 1: frequency → cubic meters per hour
    //   flow (m³/h) = (freq_Hz × 3600) / meter_factor
    //
    // meter_factor = pulses per m³ (from nameplate)
    // freq_Hz = pulses per second
    // freq_Hz × 3600 = pulses per hour
    // pulses_per_hour / pulses_per_m3 = m³/h

    float flow_m3h = (freqHz * 3600.0f) / METER_FACTOR;

    // Step 2: cubic meters per hour → liters per minute
    //   1 m³ = 1000 liters
    //   1 hour = 60 minutes
    //   flow (L/min) = flow (m³/h) × 1000 / 60

    float flow_lpm = flow_m3h * 1000.0f / 60.0f;

    return flow_lpm;
}

float FlowMeter::applyFilter(float rawValue)
{
    if (!_filterInitialized)
    {
        // Seed filter with first reading
        _frequency = rawValue;
        _filterInitialized = true;
        return rawValue;
    }

    // Exponential Moving Average:
    //   filtered = alpha * raw + (1 - alpha) * previous
    float filtered = FREQ_EMA_ALPHA * rawValue
                   + (1.0f - FREQ_EMA_ALPHA) * _frequency;

    return filtered;
}

bool FlowMeter::validateSensor(float freqHz) const
{
    // Reject unreasonably high frequencies (noise / wiring fault)
    if (freqHz > FREQ_MAX_VALID_HZ)
    {
        return false;
    }

    // Negative frequency is impossible but guard anyway
    if (freqHz < 0.0f)
    {
        return false;
    }

    return true;
}
