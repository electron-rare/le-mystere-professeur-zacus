#include "wifi_service.h"

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

}  // namespace

void WifiService::begin(const char* hostname) {
  snap_ = Snapshot();
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);
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
      setEvent("SCAN_START");
    } else {
      setError("SCAN_FAIL");
      scanRequested_ = false;
      setEvent("SCAN_REJECT");
    }
  }

  if (scanInFlight_ && static_cast<int32_t>(nowMs - lastScanStartMs_) >= static_cast<int32_t>(kScanPollMs)) {
    const int n = WiFi.scanComplete();
    if (n >= 0) {
      snap_.scanCount = static_cast<uint16_t>(n);
      WiFi.scanDelete();
      scanInFlight_ = false;
      setEvent("SCAN_DONE");
    } else if (n == WIFI_SCAN_FAILED) {
      scanInFlight_ = false;
      setError("SCAN_FAILED");
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
