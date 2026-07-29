#include "WebDashboard.h"
#include "dashboard_html.h"
#include "../../utils/Logger.h"

#include <ArduinoJson.h>

static const char* TAG = "Dashboard";

// Static instance for callback routing
WebDashboard* WebDashboard::_instance = nullptr;

// ============================================================
//  Constructor
// ============================================================

WebDashboard::WebDashboard()
    : _server(80),
      _ws("/ws"),
      _commandCallback(nullptr)
{
    _instance = this;
}

// ============================================================
//  Initialization
// ============================================================

void WebDashboard::begin(uint16_t port)
{
    LOG_INFO(TAG, "Starting web dashboard on port %d", port);

    // Serve the HTML dashboard on root
    _server.on("/", HTTP_GET, [](AsyncWebServerRequest* request)
    {
        request->send_P(200, "text/html", DASHBOARD_HTML);
    });

    // Attach WebSocket handler
    _ws.onEvent(onWsEvent);
    _server.addHandler(&_ws);

    // Start the server
    _server.begin();

    LOG_INFO(TAG, "Web dashboard started — WebSocket at /ws");
}

// ============================================================
//  Push Data to All Clients
// ============================================================

void WebDashboard::pushData(const char* jsonData)
{
    if (_ws.count() == 0) return;

    _ws.textAll(jsonData);
}

// ============================================================
//  Command Callback
// ============================================================

void WebDashboard::onCommand(DashboardCommandCallback cb)
{
    _commandCallback = cb;
}

// ============================================================
//  Client Cleanup
// ============================================================

void WebDashboard::cleanupClients()
{
    _ws.cleanupClients();
}

size_t WebDashboard::getClientCount() const
{
    return _ws.count();
}

// ============================================================
//  WebSocket Event Handler
// ============================================================

void WebDashboard::onWsEvent(
    AsyncWebSocket* server,
    AsyncWebSocketClient* client,
    AwsEventType type,
    void* arg,
    uint8_t* data,
    size_t len)
{
    switch (type)
    {
        case WS_EVT_CONNECT:
        {
            LOG_INFO(TAG, "WebSocket client #%u connected from %s",
                     client->id(),
                     client->remoteIP().toString().c_str());
            break;
        }

        case WS_EVT_DISCONNECT:
        {
            LOG_INFO(TAG, "WebSocket client #%u disconnected",
                     client->id());
            break;
        }

        case WS_EVT_DATA:
        {
            // Parse incoming JSON command
            AwsFrameInfo* info = (AwsFrameInfo*)arg;

            if (info->final && info->index == 0 &&
                info->len == len && info->opcode == WS_TEXT)
            {
                // Null-terminate the data
                data[len] = '\0';
                const char* payload = (const char*)data;

                LOG_DEBUG(TAG, "WS message: %s", payload);

                // Parse command
                StaticJsonDocument<128> doc;
                DeserializationError err = deserializeJson(doc, payload);

                if (err)
                {
                    LOG_WARNING(TAG, "Invalid WS JSON: %s",
                                err.c_str());
                    break;
                }

                const char* cmd = doc["cmd"] | "";
                if (strlen(cmd) > 0)
                {
                    LOG_INFO(TAG, "Dashboard command: %s", cmd);

                    // Forward to SystemManager via callback
                    if (_instance && _instance->_commandCallback)
                    {
                        _instance->_commandCallback(cmd);
                    }

                    // Send acknowledgment back to client
                    char ack[96];
                    snprintf(ack, sizeof(ack),
                             "{\"type\":\"ack\",\"message\":\"%s executed\"}",
                             cmd);
                    client->text(ack);
                }
            }
            break;
        }

        case WS_EVT_PONG:
        case WS_EVT_ERROR:
            break;
    }
}
