#include "WiFiManager.h"
#include "../../utils/Logger.h"
#include "../../utils/ErrorHandler.h"

static const char* TAG = "WiFi";

WiFiManager::WiFiManager()
    : _hostname("ESP32-DieselMonitor"),
      _state(State::DISCONNECTED),
      _connectStartTime(0),
      _lastReconnectTime(0),
      _connectedSince(0),
      _reconnectCount(0)
{
}

void WiFiManager::begin(const String& ssid,
                        const String& password)
{
    _ssid = ssid;
    _password = password;

    WiFi.mode(WIFI_STA);
    WiFi.setHostname(_hostname);
    WiFi.onEvent(wifiEvent);

    startConnection();
}

void WiFiManager::disconnect()
{
    WiFi.disconnect(true);
    _state = State::DISCONNECTED;
    LOG_INFO(TAG, "Disconnected");
}

void WiFiManager::update()
{
    checkConnection();
}

bool WiFiManager::isConnected() const
{
    return WiFi.status() == WL_CONNECTED || _state == State::AP_MODE;
}

bool WiFiManager::isAPMode() const
{
    return _state == State::AP_MODE;
}

WiFiManager::State WiFiManager::getState() const
{
    return _state;
}

IPAddress WiFiManager::getIPAddress() const
{
    if (_state == State::AP_MODE)
        return WiFi.softAPIP();

    return WiFi.localIP();
}

int WiFiManager::getRSSI() const
{
    if (!isConnected())
        return 0;

    return WiFi.RSSI();
}

uint8_t WiFiManager::getSignalQuality() const
{
    int rssi = getRSSI();

    if (rssi <= -100)
        return 0;

    if (rssi >= -50)
        return 100;

    return static_cast<uint8_t>(2 * (rssi + 100));
}

const String& WiFiManager::getSSID() const
{
    return _ssid;
}

String WiFiManager::getMACAddress() const
{
    return WiFi.macAddress();
}

unsigned long WiFiManager::getConnectedTime() const
{
    if (!isConnected())
        return 0;

    return millis() - _connectedSince;
}

uint32_t WiFiManager::getReconnectCount() const
{
    return _reconnectCount;
}

void WiFiManager::setHostname(const char* hostname)
{
    _hostname = hostname;
    WiFi.setHostname(_hostname);
}

/*==================================================
    Internal — Non-blocking connection management
==================================================*/

void WiFiManager::startConnection()
{
    LOG_INFO(TAG, "Connecting to %s...", _ssid.c_str());

    WiFi.begin(_ssid.c_str(), _password.c_str());

    _connectStartTime = millis();
    _state = State::CONNECTING;
}

void WiFiManager::checkConnection()
{
    // Already connected
    if (WiFi.status() == WL_CONNECTED)
    {
        if (_state != State::CONNECTED)
        {
            _state = State::CONNECTED;
            _connectedSince = millis();

            ErrorHandler::clearError(SystemError::ERROR_WIFI_FAILURE);

            LOG_INFO(TAG, "Connected. IP: %s",
                     WiFi.localIP().toString().c_str());
        }

        return;
    }

    // Connection attempt timed out
    if ((_state == State::CONNECTING ||
         _state == State::RECONNECTING) &&
        millis() - _connectStartTime > CONNECTION_TIMEOUT)
    {
        LOG_WARNING(TAG, "Connection timeout");
        _state = State::CONNECTION_FAILED;
        ErrorHandler::setError(SystemError::ERROR_WIFI_FAILURE);
    }

    // Time to try reconnecting (non-blocking)
    if ((_state == State::DISCONNECTED ||
         _state == State::CONNECTION_FAILED) &&
        millis() - _lastReconnectTime > RECONNECT_INTERVAL)
    {
        // Check if we should give up STA and start AP
        if (_reconnectCount >= AP_FALLBACK_ATTEMPTS)
        {
            startAP();
            return;
        }

        _lastReconnectTime = millis();
        _reconnectCount++;
        _state = State::RECONNECTING;

        LOG_INFO(TAG, "Reconnect attempt %lu / %lu",
                 (unsigned long)_reconnectCount,
                 (unsigned long)AP_FALLBACK_ATTEMPTS);

        WiFi.disconnect(true);

        // Non-blocking: WiFi.begin returns immediately
        WiFi.begin(_ssid.c_str(), _password.c_str());

        _connectStartTime = millis();
    }
}

// ============================================================
//  AP Mode Fallback
// ============================================================

void WiFiManager::startAP()
{
    LOG_WARNING(TAG, "STA connection failed after %lu attempts",
                (unsigned long)_reconnectCount);
    LOG_INFO(TAG, "Starting Access Point: %s", AP_SSID);

    WiFi.disconnect(true);
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID, AP_PASSWORD);

    _state = State::AP_MODE;

    delay(100);  // Small delay for AP to start

    LOG_INFO(TAG, "============================================");
    LOG_INFO(TAG, "  AP Mode Active");
    LOG_INFO(TAG, "  SSID:     %s", AP_SSID);
    LOG_INFO(TAG, "  Password: %s", AP_PASSWORD);
    LOG_INFO(TAG, "  Dashboard: http://%s/",
             WiFi.softAPIP().toString().c_str());
    LOG_INFO(TAG, "============================================");
}

void WiFiManager::wifiEvent(WiFiEvent_t event)
{
    switch (event)
    {
        case ARDUINO_EVENT_WIFI_STA_CONNECTED:
            LOG_INFO(TAG, "Associated with AP");
            break;

        case ARDUINO_EVENT_WIFI_STA_GOT_IP:
            LOG_INFO(TAG, "IP address acquired");
            break;

        case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
            LOG_WARNING(TAG, "Disconnected from AP");
            break;

        default:
            break;
    }
}
