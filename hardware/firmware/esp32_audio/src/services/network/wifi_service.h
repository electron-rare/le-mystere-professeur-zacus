#pragma once

#include <Arduino.h>

class WifiService {
 public:
  struct Snapshot {
    bool staConnected = false;
    bool apEnabled = false;
    bool scanning = false;
    char mode[16] = "OFF";
    char ssid[33] = "";
    char ip[20] = "0.0.0.0";
    int32_t rssi = 0;
    uint16_t scanCount = 0;
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

 private:
  void setEvent(const char* event);
  void setError(const char* error);
  void updateSnapshot(uint32_t nowMs);

  Snapshot snap_;
  bool scanRequested_ = false;
  bool scanInFlight_ = false;
  bool apAutoFallback_ = true;
  uint32_t lastStaAttemptMs_ = 0U;
  uint32_t lastScanStartMs_ = 0U;
};
