#include "wifi_service.h"

#include <ArduinoJson.h>
#include <WiFi.h>

namespace {

constexpr uint32_t kScanPollMs = 300U;
constexpr uint32_t kApFallbackDelayMs = 12000U;

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

const char* wifiModeLabel(wifi_mode_t mode) {
  switch (mode) {
    case WIFI_MODE_NULL:
      return "OFF";
    case WIFI_MODE_STA:
      return "STA";
    case WIFI_MODE_AP:
      return "AP";
    case WIFI_MODE_APSTA:
      return "AP_STA";
    default:
      return "UNKNOWN";
  }
}

const char* wifiDisconnectReasonLabel(uint8_t reason) {
  switch (reason) {
    case WIFI_REASON_UNSPECIFIED:
      return "UNSPEC";
    case WIFI_REASON_AUTH_EXPIRE:
      return "AUTH_EXPIRE";
    case WIFI_REASON_AUTH_FAIL:
      return "AUTH_FAIL";
    case WIFI_REASON_ASSOC_FAIL:
      return "ASSOC_FAIL";
    case WIFI_REASON_NO_AP_FOUND:
      return "NO_AP";
    case WIFI_REASON_BEACON_TIMEOUT:
      return "BEACON_TIMEOUT";
    case WIFI_REASON_HANDSHAKE_TIMEOUT:
      return "HANDSHAKE_TIMEOUT";
    case WIFI_REASON_CONNECTION_FAIL:
      return "CONNECTION_FAIL";
    case WIFI_REASON_ROAMING:
      return "ROAMING";
    default:
      return "OTHER";
  }
}

}  // namespace

void WifiService::begin(const char* hostname) {
  snap_ = Snapshot();
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);
  if (!eventRegistered_) {
    eventId_ = WiFi.onEvent([this](WiFiEvent_t event, WiFiEventInfo_t info) {
      handleEvent(event, info);
    });
    eventRegistered_ = true;
  }
  if (hostname != nullptr && hostname[0] != '\0') {
    WiFi.setHostname(hostname);
  }
  WiFi.disconnect(true, true);
  setEvent("BEGIN");
}

void WifiService::update(uint32_t nowMs) {
  if (scanRequested_ && !scanInFlight_) {
    const int started = WiFi.scanNetworks(true, true);
    if (started >= 0) {
      scanInFlight_ = true;
      scanRequested_ = false;
      lastScanStartMs_ = nowMs;
      scanFailed_ = false;
      scanJson_ = "";
      setEvent("SCAN_START");
    } else {
      setError("SCAN_FAIL");
      scanRequested_ = false;
      scanFailed_ = true;
      scanJson_ = "{\"status\":\"fail\",\"count\":0,\"results\":[]}";
      setEvent("SCAN_REJECT");
    }
  }

  if (scanInFlight_ && static_cast<int32_t>(nowMs - lastScanStartMs_) >= static_cast<int32_t>(kScanPollMs)) {
    const int n = WiFi.scanComplete();
    if (n >= 0) {
      snap_.scanCount = static_cast<uint16_t>(n);
      const int maxList = 12;
      const int total = (n > maxList) ? maxList : n;
      StaticJsonDocument<2048> doc;
      doc["status"] = "ready";
      doc["count"] = n;
      JsonArray arr = doc.createNestedArray("results");
      for (int i = 0; i < total; ++i) {
        JsonObject item = arr.createNestedObject();
        item["ssid"] = WiFi.SSID(i);
        item["rssi"] = WiFi.RSSI(i);
        item["secure"] = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
      }
      String json;
      serializeJson(doc, json);
      scanJson_ = json;
      WiFi.scanDelete();
      scanInFlight_ = false;
      setEvent("SCAN_DONE");
    } else if (n == WIFI_SCAN_FAILED) {
      scanInFlight_ = false;
      setError("SCAN_FAILED");
      scanFailed_ = true;
      scanJson_ = "{\"status\":\"fail\",\"count\":0,\"results\":[]}";
      setEvent("SCAN_FAIL");
    }
  }

  if (apAutoFallback_ && WiFi.status() != WL_CONNECTED && !snap_.apEnabled &&
      static_cast<int32_t>(nowMs - lastStaAttemptMs_) >= static_cast<int32_t>(kApFallbackDelayMs)) {
    enableAp("U-SON-RADIO", "usonradio", "AP_FALLBACK");
  }

  updateSnapshot(nowMs);
}

bool WifiService::requestScan(const char* reason) {
  (void)reason;
  if (scanInFlight_) {
    return false;
  }
  scanRequested_ = true;
  scanFailed_ = false;
  scanJson_ = "";
  setEvent("SCAN_REQ");
  return true;
}

bool WifiService::connectSta(const char* ssid, const char* pass, const char* reason) {
  if (ssid == nullptr || ssid[0] == '\0') {
    setError("SSID_EMPTY");
    return false;
  }

  WiFi.mode(snap_.apEnabled ? WIFI_AP_STA : WIFI_STA);
  WiFi.begin(ssid, (pass != nullptr) ? pass : "");
  lastStaAttemptMs_ = millis();
  copyText(snap_.ssid, sizeof(snap_.ssid), ssid);
  setEvent((reason != nullptr) ? reason : "STA_CONNECT");
  return true;
}

bool WifiService::enableAp(const char* ssid, const char* pass, const char* reason) {
  const char* apSsid = (ssid != nullptr && ssid[0] != '\0') ? ssid : "U-SON-RADIO";
  const char* apPass = (pass != nullptr) ? pass : "usonradio";

  WiFi.mode(WIFI_AP_STA);
  const bool ok = WiFi.softAP(apSsid, apPass);
  if (ok) {
    snap_.apEnabled = true;
    setEvent((reason != nullptr) ? reason : "AP_ON");
  } else {
    setError("AP_FAIL");
  }
  return ok;
}

void WifiService::disableAp(const char* reason) {
  WiFi.softAPdisconnect(true);
  snap_.apEnabled = false;
  WiFi.mode(WIFI_STA);
  setEvent((reason != nullptr) ? reason : "AP_OFF");
}

WifiService::Snapshot WifiService::snapshot() const {
  return snap_;
}

bool WifiService::isConnected() const {
  return snap_.staConnected;
}

bool WifiService::isApEnabled() const {
  return snap_.apEnabled;
}

WifiService::ScanStatus WifiService::scanStatus() const {
  if (scanInFlight_ || scanRequested_) {
    return ScanStatus::Scanning;
  }
  if (scanFailed_) {
    return ScanStatus::Failed;
  }
  if (scanJson_.length() > 0U) {
    return ScanStatus::Ready;
  }
  return ScanStatus::Idle;
}

const String& WifiService::scanJson() const {
  return scanJson_;
}

void WifiService::setEvent(const char* event) {
  copyText(snap_.lastEvent, sizeof(snap_.lastEvent), event);
}

void WifiService::setError(const char* error) {
  copyText(snap_.lastError, sizeof(snap_.lastError), error);
}

void WifiService::updateSnapshot(uint32_t nowMs) {
  (void)nowMs;
  snap_.scanning = scanInFlight_ || scanRequested_;
  snap_.staConnected = (WiFi.status() == WL_CONNECTED);
  snap_.rssi = snap_.staConnected ? WiFi.RSSI() : 0;
  snap_.apEnabled = WiFi.getMode() == WIFI_MODE_AP || WiFi.getMode() == WIFI_MODE_APSTA;
  copyText(snap_.mode, sizeof(snap_.mode), wifiModeLabel(WiFi.getMode()));

  if (snap_.staConnected) {
    const IPAddress ip = WiFi.localIP();
    snprintf(snap_.ip, sizeof(snap_.ip), "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
  } else if (snap_.apEnabled) {
    const IPAddress ip = WiFi.softAPIP();
    snprintf(snap_.ip, sizeof(snap_.ip), "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
  } else {
    copyText(snap_.ip, sizeof(snap_.ip), "0.0.0.0");
  }
}

void WifiService::handleEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
  switch (event) {
#if defined(ARDUINO_EVENT_WIFI_STA_CONNECTED)
    case ARDUINO_EVENT_WIFI_STA_CONNECTED:
#else
    case SYSTEM_EVENT_STA_CONNECTED:
#endif
      setEvent("STA_CONNECTED");
      break;
#if defined(ARDUINO_EVENT_WIFI_STA_GOT_IP)
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
#else
    case SYSTEM_EVENT_STA_GOT_IP:
#endif
      setEvent("STA_GOT_IP");
      break;
#if defined(ARDUINO_EVENT_WIFI_STA_DISCONNECTED)
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED: {
#else
    case SYSTEM_EVENT_STA_DISCONNECTED: {
#endif
      const uint8_t reason = static_cast<uint8_t>(info.wifi_sta_disconnected.reason);
      ++snap_.disconnectCount;
      snap_.disconnectReason = reason;
      snap_.lastDisconnectMs = millis();
      copyText(snap_.disconnectLabel, sizeof(snap_.disconnectLabel), wifiDisconnectReasonLabel(reason));
      setEvent("STA_DISCONNECT");
      setError("STA_DISCONNECT");
      Serial.printf("[WIFI] disconnect reason=%u label=%s\n",
                    static_cast<unsigned int>(reason),
                    snap_.disconnectLabel);
      break;
    }
    default:
      break;
  }
}
