#include "web_ui_service.h"

#include <WebServer.h>

#include "../network/wifi_service.h"
#include "../radio/radio_service.h"
#include "../../audio/mp3_player.h"

namespace {

void copyText(char* out, size_t outLen, const char* text) {
  if (out == nullptr || outLen == 0U) {
    return;
  }
  out[0] = '\0';
  if (text == nullptr || text[0] == '\0') {
    return;
  }
  snprintf(out, outLen, "%s", text);
}

}  // namespace

void WebUiService::begin(WifiService* wifi, RadioService* radio, Mp3Player* mp3, uint16_t port) {
  wifi_ = wifi;
  radio_ = radio;
  mp3_ = mp3;
  snap_ = Snapshot();
  snap_.port = port;

  if (server_ != nullptr) {
    delete server_;
    server_ = nullptr;
  }

  server_ = new WebServer(port);
  if (server_ == nullptr) {
    setError("ALLOC_FAIL");
    return;
  }

  setupRoutes();
  server_->begin();
  snap_.started = true;
  setRoute("BEGIN");
}

void WebUiService::update(uint32_t nowMs) {
  (void)nowMs;
  if (server_ == nullptr || !snap_.started) {
    return;
  }
  server_->handleClient();
}

WebUiService::Snapshot WebUiService::snapshot() const {
  return snap_;
}

void WebUiService::setupRoutes() {
  if (server_ == nullptr) {
    return;
  }

  server_->on("/", HTTP_GET, [this]() {
    setRoute("/");
    ++snap_.requestCount;
    const char* html =
        "<html><head><meta charset='utf-8'><title>U-SON Radio</title></head>"
        "<body><h2>U-SON RC V3</h2><p>Endpoints: /api/status /api/radio /api/wifi</p></body></html>";
    server_->send(200, "text/html", html);
  });

  server_->on("/api/status", HTTP_GET, [this]() {
    setRoute("/api/status");
    ++snap_.requestCount;

    String json = "{";
    if (wifi_ != nullptr) {
      const WifiService::Snapshot w = wifi_->snapshot();
      json += "\"wifi\":{";
      json += "\"connected\":" + String(w.staConnected ? "true" : "false") + ",";
      json += "\"ap\":" + String(w.apEnabled ? "true" : "false") + ",";
      json += "\"mode\":\"" + String(w.mode) + "\",";
      json += "\"ip\":\"" + String(w.ip) + "\"},";
    }
    if (radio_ != nullptr) {
      const RadioService::Snapshot r = radio_->snapshot();
      json += "\"radio\":{";
      json += "\"active\":" + String(r.active ? "true" : "false") + ",";
      json += "\"station\":\"" + String(r.activeStationName) + "\",";
      json += "\"state\":\"" + String(r.streamState) + "\",";
      json += "\"buffer\":" + String(r.bufferPercent) + "},";
    }
    if (mp3_ != nullptr) {
      json += "\"mp3\":{";
      json += "\"playing\":" + String(mp3_->isPlaying() ? "true" : "false") + ",";
      json += "\"tracks\":" + String(mp3_->trackCount()) + "}";
    } else {
      if (json.endsWith(",")) {
        json.remove(json.length() - 1);
      }
    }
    if (json.endsWith(",")) {
      json.remove(json.length() - 1);
    }
    json += "}";
    server_->send(200, "application/json", json);
  });

  server_->on("/api/radio", HTTP_GET, [this]() {
    setRoute("/api/radio");
    ++snap_.requestCount;
    if (radio_ == nullptr) {
      server_->send(503, "application/json", "{\"error\":\"radio_unavailable\"}");
      return;
    }
    const RadioService::Snapshot r = radio_->snapshot();
    String json = "{";
    json += "\"active\":" + String(r.active ? "true" : "false") + ",";
    json += "\"station_id\":" + String(r.activeStationId) + ",";
    json += "\"station\":\"" + String(r.activeStationName) + "\",";
    json += "\"state\":\"" + String(r.streamState) + "\",";
    json += "\"title\":\"" + String(r.title) + "\"}";
    server_->send(200, "application/json", json);
  });

  server_->on("/api/wifi", HTTP_GET, [this]() {
    setRoute("/api/wifi");
    ++snap_.requestCount;
    if (wifi_ == nullptr) {
      server_->send(503, "application/json", "{\"error\":\"wifi_unavailable\"}");
      return;
    }
    const WifiService::Snapshot w = wifi_->snapshot();
    String json = "{";
    json += "\"connected\":" + String(w.staConnected ? "true" : "false") + ",";
    json += "\"ap\":" + String(w.apEnabled ? "true" : "false") + ",";
    json += "\"mode\":\"" + String(w.mode) + "\",";
    json += "\"ip\":\"" + String(w.ip) + "\",";
    json += "\"scan_count\":" + String(w.scanCount) + "}";
    server_->send(200, "application/json", json);
  });

  server_->onNotFound([this]() {
    setRoute("404");
    ++snap_.requestCount;
    server_->send(404, "application/json", "{\"error\":\"not_found\"}");
  });
}

void WebUiService::setRoute(const char* route) {
  copyText(snap_.lastRoute, sizeof(snap_.lastRoute), route);
}

void WebUiService::setError(const char* error) {
  copyText(snap_.lastError, sizeof(snap_.lastError), error);
}
