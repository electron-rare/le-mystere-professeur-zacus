#ifndef WEB_WEB_SERVER_MANAGER_H
#define WEB_WEB_SERVER_MANAGER_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include <freertos/FreeRTOS.h>

#include <functional>

#include "core/CommandDispatcher.h"

class WebServerManager {
public:
    explicit WebServerManager(uint16_t port = 80);
    void begin();
    void handle();

    void setAuthCredentials(const String& user, const String& pass, bool persist_to_nvs = false);
    void setAuthEnabled(bool enabled);
    bool isAuthEnabled() const;
    void setRateLimitMs(uint32_t rate_limit_ms);

    void setStatusCallback(std::function<void(JsonObject)> callback);
    void setCommandExecutor(std::function<DispatchResponse(const String&)> callback);
    void setCommandValidator(std::function<bool(const String&)> callback);

private:
    AsyncWebServer server_;
    AsyncEventSource events_;
    uint32_t rate_limit_ms_;
    uint32_t last_status_push_ms_;
    String status_cache_json_;
    bool status_cache_ready_;
    portMUX_TYPE status_cache_mux_;
    bool auth_enabled_;
    bool auth_override_set_ = false;
    String auth_user_;
    String auth_pass_;
    std::function<void(JsonObject)> status_callback_;
    std::function<DispatchResponse(const String&)> command_executor_;
    std::function<bool(const String&)> command_validator_;

    static bool extractCommandId(const String& command_line, String& command_id);
    static bool isCommandRegistered(const String& command_line,
                                    const std::function<bool(const String&)>& validator);
    void registerRoutes();
    bool authenticateRequest(AsyncWebServerRequest* request) const;
    static bool extractJsonBody(AsyncWebServerRequest* request, JsonDocument& doc);
    static String toJsonString(const JsonDocument& doc);
    static bool isValidInput(const String& value, size_t max_len);
    static bool isEffectCommand(const String& command_line);
    String snapshotStatusCache(bool* ready = nullptr);
    void refreshStatusCache();
    void publishRealtimeEvent(const char* event_name, const String& payload_json);
    void publishRealtimeStatus();
    void publishDispatchEvent(const String& command_line, const DispatchResponse& res);
    void handleDispatch(AsyncWebServerRequest* request,
                        const String& command_line,
                        uint16_t success_code = 200,
                        uint16_t error_code = 400);
};

#endif  // WEB_WEB_SERVER_MANAGER_H
