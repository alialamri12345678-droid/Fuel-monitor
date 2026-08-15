# ESP32 Firmware Integration Guide

This guide details how to configure your ESP32 firmware to securely communicate with the Diesel Meter App using MQTT. It covers secure connection, Last Will and Testament (LWT), payload construction, and HMAC-SHA256 signature generation.

## 1. Prerequisites (Arduino IDE / PlatformIO)
You will need the following standard libraries installed in your ESP32 environment:
- `PubSubClient` or `AsyncMqttClient` (for MQTT)
- `WiFiClientSecure` (for TLS/SSL)
- `mbedtls` (Built into ESP32 core for HMAC-SHA256 generation)
- `ArduinoJson` (For JSON serialization)

## 2. Global Configuration
Define your device ID, broker details, and the shared secret key matching the Flutter app.

```cpp
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "mbedtls/md.h"

const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASS";

const char* mqtt_server = "broker.hivemq.com";
const int mqtt_port = 8883;

// Must match exactly what the Flutter app expects
const char* device_id = "default_esp32_01";
const char* mqtt_secret = "default_secret"; 

// Topics
String telemetryTopic = String("devices/") + device_id + "/telemetry/flow";
String lwtTopic = String("devices/") + device_id + "/status";

WiFiClientSecure espClient;
PubSubClient client(espClient);

// Persist this in RTC memory or flash if possible to prevent resetting to 0 on crash
unsigned long sequenceNumber = 1; 
```

## 3. MQTT Connection & Last Will
When connecting to the broker, ensure you set up the LWT message. If the ESP32 loses power or Wi-Fi drops unexpectedly, the broker will automatically notify the app that the device is offline.

```cpp
void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    
    // Last Will and Testament (QoS 1, Retained=true usually preferred for status)
    const char* lwtPayload = "{\"status\": \"offline\"}";
    
    // Connect with device ID, username, password, LWT topic, LWT QoS, LWT Retain, LWT Message
    if (client.connect(device_id, NULL, NULL, lwtTopic.c_str(), 1, true, lwtPayload)) {
      Serial.println("connected");
      
      // Publish "online" status immediately upon connection
      client.publish(lwtTopic.c_str(), "{\"status\": \"online\"}", true);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}
```

## 4. HMAC-SHA256 Signature Generation
The Flutter app drops any payload that is not cryptographically signed. You must generate an HMAC-SHA256 hash of the JSON string **before** adding the signature field.

```cpp
String generateHMAC(String payload, const char* key) {
  byte hmacResult[32];
  mbedtls_md_context_t ctx;
  mbedtls_md_type_t md_type = MBEDTLS_MD_SHA256;
  
  mbedtls_md_init(&ctx);
  mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(md_type), 1);
  mbedtls_md_hmac_starts(&ctx, (const unsigned char *) key, strlen(key));
  mbedtls_md_hmac_update(&ctx, (const unsigned char *) payload.c_str(), payload.length());
  mbedtls_md_hmac_finish(&ctx, hmacResult);
  mbedtls_md_free(&ctx);
  
  // Convert byte array to hex string
  String hashStr = "";
  for(int i= 0; i< sizeof(hmacResult); i++) {
    char str[3];
    sprintf(str, "%02x", (int)hmacResult[i]);
    hashStr += str;
  }
  return hashStr;
}
```

## 5. Constructing & Publishing the Payload
When a fill event is occurring, aggregate your hardware sensor readings and publish the secured JSON payload.

```cpp
void publishTelemetry(float currentFlowRate, float totalVolume) {
  // 1. Get current Unix timestamp (Requires NTP setup beforehand)
  unsigned long currentUnixTime = time(nullptr); 
  
  // 2. Create the base JSON payload (WITHOUT signature)
  StaticJsonDocument<200> doc;
  doc["flow_rate"] = currentFlowRate;
  doc["total_vol"] = totalVolume;
  doc["ts"] = currentUnixTime;
  doc["seq"] = sequenceNumber;
  
  String canonicalPayload;
  serializeJson(doc, canonicalPayload);
  
  // 3. Generate Signature
  String signature = generateHMAC(canonicalPayload, mqtt_secret);
  
  // 4. Add signature to the final JSON
  doc["signature"] = signature;
  String finalPayload;
  serializeJson(doc, finalPayload);
  
  // 5. Publish via QoS 1
  // PubSubClient uses QoS 0 for basic publish(). 
  // Note: To use strict QoS 1, you may need a library like AsyncMqttClient.
  client.publish(telemetryTopic.c_str(), finalPayload.c_str());
  
  sequenceNumber++;
}
```

## 6. Offline Caching Strategy (Important!)
Since you have local caching enabled on the ESP32 (e.g., using LittleFS), follow this flow when Wi-Fi drops during a fill event:

1. **Detect Disconnect:** In your main loop, if `!client.connected()`, write `finalPayload` strings into a file on LittleFS instead of calling `client.publish()`.
2. **Reconnection Flush:** Once `reconnect()` succeeds, open the LittleFS file.
3. **Publish Backlog:** Read the payloads line by line and publish them to MQTT.
4. **App Adjustments:** Currently, the Flutter app rejects payloads older than 5 minutes (`ts > 300` seconds delta). Because cached payloads will be older than 5 minutes when eventually sent, **you will need to remove or modify the 300-second TTL check in `lib/core/network/mqtt_service.dart`** to allow historical syncs to pass validation.
