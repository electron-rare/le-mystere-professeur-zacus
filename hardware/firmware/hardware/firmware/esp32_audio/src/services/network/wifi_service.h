#pragma once

#include <Arduino.h>
#include <WiFi.h>

class WifiService {
 public:
  enum class ScanStatus : uint8_t {
    Idle,
    Scanning,
    Ready,
    Failed,
  };

  struct Snapshot {
    bool staConnected = false;
    bool apEnabled = false;
    bool scanning = false;
    char mode[16] = "OFF";
    char ssid[33] = "";
    char ip[20] = "0.0.0.0";
    int32_t rssi = 0;
    uint16_t scanCount = 0;
    uint16_t disconnectReason = 0;
    uint32_t disconnectCount = 0U;
    uint32_t lastDisconnectMs = 0U;
    char disconnectLabel[24] = "NONE";
    char lastError[32] = "OK";
    char lastEvent[32] = "INIT";
  };

  void begin(const char* hostname);
  void update(uint32_t nowMs);
  bool requestScan(const char* reason);
  bool connectSta(const char* ssid, const char* pass, const char* reason);
  bool enableAp(const char* ssid, const char* pass, const char* reason);
  void disableAp(const char* reason);
  Snapshot snapshot() const;
  bool isConnected() const;
  bool isApEnabled() const;
  ScanStatus scanStatus() const;
  const String& scanJson() const;

 private:
  void setEvent(const char* event);
  void setError(const char* error);
  void updateSnapshot(uint32_t nowMs);
  void handleEvent(WiFiEvent_t event, WiFiEventInfo_t info);

  Snapshot snap_;
  bool scanRequested_ = false;
  bool scanInFlight_ = false;
  bool scanFailed_ = false;
  String scanJson_ = "";
  bool apAutoFallback_ = true;
  uint32_t lastStaAttemptMs_ = 0U;
  uint32_t lastScanStartMs_ = 0U;
  bool eventRegistered_ = false;
  WiFiEventId_t eventId_ = 0;
};
