#ifndef CONFIG_H
#define CONFIG_H

/**
 * ============================================================
 * Config.h
 * ------------------------------------------------------------
 * Central configuration for the Diesel Delivery Monitoring
 * System (RS485 Modbus + MQTT).
 *
 * All adjustable parameters are defined here. Modify this
 * file to change hardware pins, calibration values, network
 * credentials, and timing intervals without touching logic.
 * ============================================================
 */

// ============================================================
//  Hardware Pins
// ============================================================

// RS485 — Modbus RTU (Serial2)
#define RS485_RX_PIN            16      // Serial2 RX (from RS485 module RO)
#define RS485_TX_PIN            17      // Serial2 TX (to RS485 module DI)
#define RS485_DE_RE_PIN         4       // DE/RE flow control pin

// ============================================================
//  Modbus Configuration
// ============================================================

#define MODBUS_SLAVE_ID         1       // Transmitter Modbus address
#define MODBUS_BAUD_RATE        9600    // RS485 baud rate
#define MODBUS_POLL_INTERVAL_MS 1000    // Poll registers every 1 second

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

// Sensor read + delivery state machine update
#define SENSOR_READ_INTERVAL_MS     100

// MQTT live data publish interval
#define MQTT_PUBLISH_INTERVAL_MS    1000

// System status publish interval
#define STATUS_PUBLISH_INTERVAL_MS  30000

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
//  Serial / Debug
// ============================================================

#define SERIAL_BAUD_RATE        9600

#endif // CONFIG_H
