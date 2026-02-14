#pragma once

#include <Arduino.h>

class Mp3Player;
class WifiService;
class RadioService;
class WebServer;

class WebUiService {
 public:
  struct Snapshot {
    bool started = false;
    uint16_t port = 80U;
    uint32_t requestCount = 0U;
    char lastRoute[32] = "-";
    char lastError[32] = "OK";
  };

  void begin(WifiService* wifi, RadioService* radio, Mp3Player* mp3, uint16_t port = 80U);
  void update(uint32_t nowMs);
  Snapshot snapshot() const;

 private:
  void setupRoutes();
  void setRoute(const char* route);
  void setError(const char* error);

  WifiService* wifi_ = nullptr;
  RadioService* radio_ = nullptr;
  Mp3Player* mp3_ = nullptr;
  WebServer* server_ = nullptr;
  Snapshot snap_;
};
