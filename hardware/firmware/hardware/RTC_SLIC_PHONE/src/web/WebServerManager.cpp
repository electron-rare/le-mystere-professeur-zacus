#include "web/WebServerManager.h"

#include <FFat.h>
#include <SPIFFS.h>

namespace {
constexpr bool kForceAuthDisabled = false;
constexpr bool kEnableRealtimeEvents = true;

String quoteArg(const String& value) {
    String escaped = value;
    escaped.replace("\\", "\\\\");
    escaped.replace("\"", "\\\"");
    return String("\"") + escaped + "\"";
}
}

WebServerManager::WebServerManager(uint16_t port)
    : server_(port),
      events_("/api/events"),
      rate_limit_ms_(250),
      last_status_push_ms_(0),
      status_cache_json_(""),
      status_cache_ready_(false),
      status_cache_mux_(portMUX_INITIALIZER_UNLOCKED),
      auth_enabled_(true),
      auth_user_("admin"),
      auth_pass_("admin") {}

void WebServerManager::begin() {
#if defined(BOARD_PROFILE_A252)
    if (FFat.begin(false) || FFat.begin(true)) {
        server_.serveStatic("/", FFat, "/webui/").setDefaultFile("index.html");
    } else {
        Serial.println("[WebServerManager] FFat mount failed");
    }
#elif defined(USB_MSC_BOOT_ENABLE)
    if (FFat.begin(false, "/usbmsc", 10, "usbmsc") || FFat.begin(true, "/usbmsc", 10, "usbmsc")) {
        server_.serveStatic("/", FFat, "/webui/").setDefaultFile("index.html");
    } else {
        Serial.println("[WebServerManager] FFat mount failed (label usbmsc)");
    }
#else
    if (!SPIFFS.begin(false) && !SPIFFS.begin(true)) {
        Serial.println("[WebServerManager] SPIFFS mount failed");
    } else {
        server_.serveStatic("/", SPIFFS, "/webui/").setDefaultFile("index.html");
    }
#endif
    registerRoutes();
    server_.begin();
    Serial.println("[WebServerManager] HTTP server started");
}

void WebServerManager::handle() {
    const uint32_t now = millis();
    if (now - last_status_push_ms_ >= 1000U) {
        last_status_push_ms_ = now;
        refreshStatusCache();
        publishRealtimeStatus();
    }
}

void WebServerManager::setAuthCredentials(const String& user, const String& pass, bool persist_to_nvs) {
    (void)persist_to_nvs;
    if (!isValidInput(user, 32) || !isValidInput(pass, 64)) {
        return;
    }
    auth_user_ = user;
    auth_pass_ = pass;
}

void WebServerManager::setAuthEnabled(bool enabled) {
    if (kForceAuthDisabled) {
        auth_enabled_ = false;
        auth_override_set_ = true;
        return;
    }
    auth_override_set_ = true;
    auth_enabled_ = enabled;
}

bool WebServerManager::isAuthEnabled() const {
    if (kForceAuthDisabled && !auth_override_set_) {
        return false;
    }
    return auth_enabled_;
}

void WebServerManager::setCommandValidator(std::function<bool(const String&)> callback) {
    command_validator_ = std::move(callback);
}

void WebServerManager::setRateLimitMs(uint32_t rate_limit_ms) {
    rate_limit_ms_ = rate_limit_ms;
}

void WebServerManager::setStatusCallback(std::function<void(JsonObject)> callback) {
    status_callback_ = std::move(callback);
}

void WebServerManager::setCommandExecutor(std::function<DispatchResponse(const String&)> callback) {
    command_executor_ = std::move(callback);
}

void WebServerManager::registerRoutes() {
    if (kEnableRealtimeEvents) {
        events_.onConnect([this](AsyncEventSourceClient* client) {
            JsonDocument hello;
            hello["transport"] = "sse";
            hello["connected"] = true;
            hello["ts"] = millis();
            const String payload = toJsonString(hello);
            client->send(payload.c_str(), "hello", millis());
            bool ready = false;
            const String cached = snapshotStatusCache(&ready);
            if (ready) {
                client->send(cached.c_str(), "status", millis());
            }
        });
        server_.addHandler(&events_);
    }

    server_.on("/api/status", HTTP_GET, [this](AsyncWebServerRequest* request) {
        bool ready = false;
        const String cached = snapshotStatusCache(&ready);
        if (ready) {
            request->send(200, "application/json", cached);
            return;
        }

        JsonDocument warmup;
        warmup["auth_enabled"] = isAuthEnabled();
        warmup["state"] = "status_warmup";
        request->send(200, "application/json", toJsonString(warmup));
    });

    server_.on("/api/control", HTTP_POST, [this](AsyncWebServerRequest* request) {
        JsonDocument doc;
        if (!extractJsonBody(request, doc)) {
            request->send(400, "application/json", "{\"error\":\"invalid json body\"}");
            return;
        }
        const String action = doc["action"] | "";
        if (!isValidInput(action, 128)) {
            request->send(400, "application/json", "{\"error\":\"invalid action\"}");
            return;
        }
        handleDispatch(request, action);
    });

    // A252 config endpoints.
    server_.on("/api/config/pins", HTTP_GET,
               [this](AsyncWebServerRequest* request) { handleDispatch(request, "SLIC_CONFIG_GET"); });
    server_.on("/api/config/pins", HTTP_POST, [this](AsyncWebServerRequest* request) {
        JsonDocument doc;
        if (!extractJsonBody(request, doc)) {
            request->send(400, "application/json", "{\"error\":\"invalid json body\"}");
            return;
        }
        String payload;
        serializeJson(doc, payload);
        handleDispatch(request, "SLIC_CONFIG_SET " + payload);
    });

    server_.on("/api/config/audio", HTTP_GET,
               [this](AsyncWebServerRequest* request) { handleDispatch(request, "AUDIO_CONFIG_GET"); });
    server_.on("/api/config/audio", HTTP_POST, [this](AsyncWebServerRequest* request) {
        JsonDocument doc;
        if (!extractJsonBody(request, doc)) {
            request->send(400, "application/json", "{\"error\":\"invalid json body\"}");
            return;
        }
        String payload;
        serializeJson(doc, payload);
        handleDispatch(request, "AUDIO_CONFIG_SET " + payload);
    });

    // WiFi.
    server_.on("/api/network/wifi", HTTP_GET,
               [this](AsyncWebServerRequest* request) { handleDispatch(request, "WIFI_STATUS"); });
    server_.on("/api/network/wifi/connect", HTTP_POST, [this](AsyncWebServerRequest* request) {
        JsonDocument doc;
        if (!extractJsonBody(request, doc)) {
            request->send(400, "application/json", "{\"error\":\"invalid json body\"}");
            return;
        }
        const String ssid = doc["ssid"] | "";
        const String pass = doc["pass"] | "";
        if (!isValidInput(ssid, 64)) {
            request->send(400, "application/json", "{\"error\":\"invalid ssid\"}");
            return;
        }
        handleDispatch(request, "WIFI_CONNECT " + quoteArg(ssid) + " " + quoteArg(pass));
    });
    server_.on("/api/network/wifi/disconnect", HTTP_POST,
               [this](AsyncWebServerRequest* request) { handleDispatch(request, "WIFI_DISCONNECT"); });
    server_.on("/api/network/wifi/reconnect", HTTP_POST,
               [this](AsyncWebServerRequest* request) { handleDispatch(request, "WIFI_RECONNECT"); });
    server_.on("/api/network/wifi/scan", HTTP_POST,
               [this](AsyncWebServerRequest* request) { handleDispatch(request, "WIFI_SCAN"); });

    // ESP-NOW.
    server_.on("/api/network/espnow", HTTP_GET,
               [this](AsyncWebServerRequest* request) { handleDispatch(request, "ESPNOW_STATUS"); });
    server_.on("/api/network/espnow/on", HTTP_POST,
               [this](AsyncWebServerRequest* request) { handleDispatch(request, "ESPNOW_ON"); });
    server_.on("/api/network/espnow/off", HTTP_POST,
               [this](AsyncWebServerRequest* request) { handleDispatch(request, "ESPNOW_OFF"); });
    server_.on("/api/network/espnow/peer", HTTP_GET,
               [this](AsyncWebServerRequest* request) { handleDispatch(request, "ESPNOW_PEER_LIST"); });
    server_.on("/api/network/espnow/peer", HTTP_POST, [this](AsyncWebServerRequest* request) {
        JsonDocument doc;
        if (!extractJsonBody(request, doc)) {
            request->send(400, "application/json", "{\"error\":\"invalid json body\"}");
            return;
        }
        const String mac = doc["mac"] | "";
        if (!isValidInput(mac, 32)) {
            request->send(400, "application/json", "{\"error\":\"invalid mac\"}");
            return;
        }
        handleDispatch(request, "ESPNOW_PEER_ADD " + mac);
    });
    server_.on("/api/network/espnow/peer", HTTP_DELETE, [this](AsyncWebServerRequest* request) {
        JsonDocument doc;
        if (!extractJsonBody(request, doc)) {
            request->send(400, "application/json", "{\"error\":\"invalid json body\"}");
            return;
        }
        const String mac = doc["mac"] | "";
        if (!isValidInput(mac, 32)) {
            request->send(400, "application/json", "{\"error\":\"invalid mac\"}");
            return;
        }
        handleDispatch(request, "ESPNOW_PEER_DEL " + mac);
    });
    server_.on("/api/network/espnow/send", HTTP_POST, [this](AsyncWebServerRequest* request) {
        JsonDocument doc;
        if (!extractJsonBody(request, doc)) {
            request->send(400, "application/json", "{\"error\":\"invalid json body\"}");
            return;
        }
        const String mac = doc["mac"] | "";
        if (!isValidInput(mac, 32)) {
            request->send(400, "application/json", "{\"error\":\"invalid mac\"}");
            return;
        }
        String payload;
        JsonVariantConst payload_variant = doc["payload"].as<JsonVariantConst>();
        bool already_enveloped = false;
        if (payload_variant.is<JsonObjectConst>()) {
            JsonObjectConst payload_obj = payload_variant.as<JsonObjectConst>();
            already_enveloped = payload_obj["msg_id"].is<const char*>() && payload_obj["type"].is<const char*>();
        }

        if (already_enveloped) {
            serializeJson(payload_variant, payload);
        } else {
            JsonDocument envelope;
            envelope["msg_id"] = String("web-") + String(millis());
            envelope["seq"] = millis();
            envelope["type"] = "command";
            envelope["ack"] = true;
            if (!payload_variant.isNull()) {
                envelope["payload"].set(payload_variant);
            } else {
                envelope["payload"].to<JsonObject>();
            }
            serializeJson(envelope, payload);
        }
        handleDispatch(request, "ESPNOW_SEND " + mac + " " + payload);
    });

}

bool WebServerManager::authenticateRequest(AsyncWebServerRequest* request) const {
    if (kForceAuthDisabled || !auth_enabled_) {
        return true;
    }
    if (!request->authenticate(auth_user_.c_str(), auth_pass_.c_str())) {
        request->requestAuthentication();
        return false;
    }
    return true;
}

bool WebServerManager::extractJsonBody(AsyncWebServerRequest* request, JsonDocument& doc) {
    if (request->hasParam("plain", true)) {
        const String body = request->getParam("plain", true)->value();
        return deserializeJson(doc, body) == DeserializationError::Ok;
    }
    return false;
}

String WebServerManager::toJsonString(const JsonDocument& doc) {
    String out;
    serializeJson(doc, out);
    return out;
}

bool WebServerManager::isValidInput(const String& value, size_t max_len) {
    if (value.isEmpty() || value.length() > max_len) {
        return false;
    }
    for (size_t i = 0; i < value.length(); ++i) {
        const char c = value[i];
        if (c < 32 || c > 126) {
            return false;
        }
    }
    return true;
}

bool WebServerManager::isEffectCommand(const String& command_line) {
    String token = command_line;
    const int sep = token.indexOf(' ');
    if (sep > 0) {
        token = token.substring(0, sep);
    }
    token.trim();
    token.toUpperCase();

    return token == "CALL" || token == "PLAY" || token == "CAPTURE_START" || token == "CAPTURE_STOP";
}

bool WebServerManager::extractCommandId(const String& command_line, String& command_id) {
    command_id = "";
    String line = command_line;
    line.trim();
    if (line.isEmpty()) {
        return false;
    }

    int sep = -1;
    const int len = line.length();
    bool in_quote = false;
    bool escaped = false;
    for (int i = 0; i < len; ++i) {
        const char c = line[i];
        if (in_quote) {
            if (escaped) {
                escaped = false;
            } else if (c == '\\') {
                escaped = true;
            } else if (c == '"') {
                in_quote = false;
            }
            continue;
        }
        if (c == '"') {
            in_quote = true;
            continue;
        }
        if (c == ' ') {
            sep = i;
            break;
        }
    }

    if (sep < 0) {
        command_id = line;
    } else {
        command_id = line.substring(0, sep);
    }
    command_id.trim();
    command_id.toUpperCase();
    return !command_id.isEmpty();
}

bool WebServerManager::isCommandRegistered(const String& command_line,
                                           const std::function<bool(const String&)>& validator) {
    if (!validator) {
        return true;
    }
    String command_id;
    if (!extractCommandId(command_line, command_id)) {
        return false;
    }
    return validator(command_id);
}

void WebServerManager::refreshStatusCache() {
    if (!status_callback_) {
        portENTER_CRITICAL(&status_cache_mux_);
        status_cache_ready_ = false;
        status_cache_json_ = "";
        portEXIT_CRITICAL(&status_cache_mux_);
        return;
    }

    JsonDocument doc;
    doc["auth_enabled"] = isAuthEnabled();
    status_callback_(doc.to<JsonObject>());
    const String payload = toJsonString(doc);

    portENTER_CRITICAL(&status_cache_mux_);
    status_cache_json_ = payload;
    status_cache_ready_ = true;
    portEXIT_CRITICAL(&status_cache_mux_);
}

String WebServerManager::snapshotStatusCache(bool* ready) {
    portENTER_CRITICAL(&status_cache_mux_);
    const bool has_data = status_cache_ready_;
    const String payload = status_cache_json_;
    portEXIT_CRITICAL(&status_cache_mux_);
    if (ready != nullptr) {
        *ready = has_data;
    }
    return payload;
}

void WebServerManager::publishRealtimeEvent(const char* event_name, const String& payload_json) {
    if (!kEnableRealtimeEvents) {
        return;
    }
    events_.send(payload_json.c_str(), event_name, millis());
}

void WebServerManager::publishRealtimeStatus() {
    bool ready = false;
    const String cached = snapshotStatusCache(&ready);
    if (!ready) {
        return;
    }
    publishRealtimeEvent("status", cached);
}

void WebServerManager::publishDispatchEvent(const String& command_line, const DispatchResponse& res) {
    JsonDocument doc;
    doc["command"] = command_line;
    doc["ok"] = res.ok;
    if (!res.code.isEmpty()) {
        doc["code"] = res.code;
    }
    if (!res.raw.isEmpty()) {
        doc["raw"] = res.raw;
    }
    if (!res.json.isEmpty()) {
        JsonDocument parsed;
        if (deserializeJson(parsed, res.json) == DeserializationError::Ok) {
            doc["json"].set(parsed.as<JsonVariantConst>());
        } else {
            doc["json_raw"] = res.json;
        }
    }

    const String payload = toJsonString(doc);
    publishRealtimeEvent("dispatch", payload);
    if (isEffectCommand(command_line)) {
        publishRealtimeEvent("effect", payload);
    }
}

void WebServerManager::handleDispatch(AsyncWebServerRequest* request,
                                     const String& command_line,
                                     uint16_t success_code,
                                     uint16_t error_code) {
    if (!authenticateRequest(request)) {
        return;
    }
    if (!command_executor_) {
        request->send(500, "application/json", "{\"error\":\"command executor not configured\"}");
        return;
    }

    if (!isCommandRegistered(command_line, command_validator_)) {
        JsonDocument invalid;
        invalid["ok"] = false;
        invalid["error"] = "unsupported_command";
        invalid["command"] = command_line;
        invalid["path"] = request->url();
        request->send(400, "application/json", toJsonString(invalid));
        return;
    }

    const DispatchResponse res = command_executor_(command_line);

    if (!res.json.isEmpty()) {
        request->send(res.ok ? success_code : error_code, "application/json", res.json);
    } else {
        JsonDocument doc;
        doc["ok"] = res.ok;
        if (!res.code.isEmpty()) {
            doc["code"] = res.code;
        }
        if (!res.raw.isEmpty()) {
            doc["raw"] = res.raw;
        }
        request->send(res.ok ? success_code : error_code, "application/json", toJsonString(doc));
    }

    publishDispatchEvent(command_line, res);
}
