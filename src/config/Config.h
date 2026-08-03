#ifndef CONFIG_H
#define CONFIG_H

/**
 * ============================================================
 * Config.h
 * ------------------------------------------------------------
 * Central configuration for the Diesel Delivery Verification
 * & Monitoring System.
 *
 * All adjustable parameters are defined here. Modify this
 * file to change hardware pins, calibration values, network
 * credentials, and timing intervals without touching logic.
 * ============================================================
 */

// ============================================================
//  Hardware Pins
// ============================================================

// Pulse Input — Flow meter frequency output (Fout)
// Open-collector pulled up to 3.3V with 10kΩ resistor
// Must be interrupt-capable GPIO (input-only pins OK)
#define PULSE_INPUT_PIN         34    // GPIO 34 — interrupt-capable

// I2C — DS3231 RTC
#define I2C_SDA_PIN             21      // Default I2C SDA
#define I2C_SCL_PIN             22      // Default I2C SCL

// SPI — SD Card Module (VSPI)
#define SD_CS_PIN               5       // Chip Select
#define SD_MOSI_PIN             23      // MOSI (default VSPI)
#define SD_MISO_PIN             19      // MISO (default VSPI)
#define SD_SCK_PIN              18      // SCK  (default VSPI)

// RS485 — Modbus RTU (Serial2)
#define RS485_RX_PIN            16      // Serial2 RX (from RS485 module RO)
#define RS485_TX_PIN            17      // Serial2 TX (to RS485 module DI)
#define RS485_DE_RE_PIN         4       // DE/RE flow control pin

// ============================================================
//  Flow Source Selection
// ============================================================
// FLOW_SOURCE_PULSE  = Use GPIO pulse/frequency counting
// FLOW_SOURCE_MODBUS = Use Modbus RTU via RS485
#define FLOW_SOURCE_PULSE       0
#define FLOW_SOURCE_MODBUS      1
#define CURRENT_FLOW_SOURCE     FLOW_SOURCE_MODBUS

// ============================================================
//  Modbus Configuration
// ============================================================

#define MODBUS_SLAVE_ID         1       // Transmitter Modbus address
#define MODBUS_BAUD_RATE        9600    // RS485 baud rate
#define MODBUS_POLL_INTERVAL_MS 1000    // Poll registers every 1 second

// ============================================================
//  Flow Meter — Pulse/Frequency Configuration
// ============================================================

// Meter factor from nameplate (pulses per cubic meter)
// At max flow (100 m³/h): frequency = 100 * 6836 / 3600 ≈ 1899 Hz
#define METER_FACTOR            6836.0f

// Pulse measurement window (milliseconds)
// Pulses are counted over this window, then converted to frequency
#define PULSE_WINDOW_MS         1000

// Minimum debounce interval between valid pulses (microseconds)
// At max flow (1899 Hz): period ≈ 526 µs
// Must be LESS than min period to avoid missing real pulses!
// 400µs gives margin: max countable = 2500 Hz
#define PULSE_DEBOUNCE_US       526

// EMA filter alpha for smoothing frequency (0.0–1.0)
// Higher = faster response, noisier. Lower = smoother, slower.
#define FREQ_EMA_ALPHA          0.3f

// Maximum valid frequency (Hz) — readings above this are rejected
// At 100 m³/h with meter_factor 6836: ~1899 Hz
// Set ceiling with safety margin
#define FREQ_MAX_VALID_HZ       2500.0f

// Minimum flow rate to consider as "flowing" (L/min)
// Below this, flow is treated as zero (noise floor)
// 1 stray pulse/sec = ~8.8 L/min, so set above that
#define FLOW_MIN_THRESHOLD_LPM  20.0f

// ============================================================
//  Delivery Detection
// ============================================================

// Flow rate threshold to detect delivery (L/min)
// Delivery starts when flow exceeds START, ends when below END
// for DELIVERY_END_DELAY_SECONDS continuously
#define DELIVERY_START_THRESHOLD_LPM    5.0f
#define DELIVERY_END_THRESHOLD_LPM      2.0f

// Seconds that flow must remain below threshold
// before delivery is considered complete
#define DELIVERY_END_DELAY_SECONDS  20

// ============================================================
//  Timing Intervals (milliseconds)
// ============================================================

// Sensor ADC read + flow calculation
#define SENSOR_READ_INTERVAL_MS     100

// SD card CSV logging interval
#define LOG_INTERVAL_MS             1000

// MQTT live data publish interval
#define MQTT_PUBLISH_INTERVAL_MS    1000

// WiFi monitoring (checked every loop, but reconnect has backoff)
// No define needed — runs every loop cycle

// System status publish interval
#define STATUS_PUBLISH_INTERVAL_MS  30000

// Offline sync check interval
#define OFFLINE_SYNC_INTERVAL_MS    5000

// ============================================================
//  WiFi Configuration
// ============================================================

#define WIFI_SSID               "AliAmri"
#define WIFI_PASSWORD           "liano62512"
#define WIFI_HOSTNAME           "ESP32-DieselMonitor"

// ============================================================
//  MQTT Configuration
// ============================================================

#define MQTT_BROKER             "broker.emqx.io"
#define MQTT_PORT               1883
#define MQTT_USERNAME           "mqtt_user"
#define MQTT_PASSWORD           "mqtt_pass"
#define MQTT_DEVICE_ID          "DIESEL001"

// MQTT buffer size for PubSubClient
#define MQTT_BUFFER_SIZE        2048

// MQTT topic prefix
#define MQTT_TOPIC_PREFIX       "diesel/device"

// ============================================================
//  SD Card — File Paths
// ============================================================

#define FLOW_LOG_FILE           "/flow_log.csv"
#define DELIVERY_SUMMARY_FILE   "/delivery_summary.csv"
#define PENDING_DELIVERY_FILE   "/pending_deliveries.dat"
#define DELIVERY_ID_FILE        "/delivery_id.dat"

// Maximum offline buffered deliveries
#define MAX_PENDING_DELIVERIES  500

// ============================================================
//  Serial / Debug
// ============================================================

#define SERIAL_BAUD_RATE        9600

#endif // CONFIG_H
