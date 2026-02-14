#pragma once

#include <Arduino.h>

class Mp3Player;
class WifiService;
class RadioService;
class AsyncWebServer;
class AsyncWebServerRequest;

class WebUiService {
 public:
  struct Config {
    bool authEnabled = false;
    char user[33] = "admin";
    char pass[65] = "usonradio";
  };

  struct Snapshot {
    bool started = false;
    uint16_t port = 80U;
    uint32_t requestCount = 0U;
    char lastRoute[32] = "-";
    char lastError[32] = "OK";
  };

  void begin(WifiService* wifi,
             RadioService* radio,
             Mp3Player* mp3,
             uint16_t port = 80U,
             const Config* cfg = nullptr);
  void update(uint32_t nowMs);
  Snapshot snapshot() const;

 private:
  void setupRoutes();
  bool checkAuth(AsyncWebServerRequest* request);
  void sendJsonStatus(AsyncWebServerRequest* request);
  void sendJsonPlayer(AsyncWebServerRequest* request);
  void sendJsonRadio(AsyncWebServerRequest* request);
  void sendJsonWifi(AsyncWebServerRequest* request);
  void setRoute(const char* route);
  void setError(const char* error);

  WifiService* wifi_ = nullptr;
  RadioService* radio_ = nullptr;
  Mp3Player* mp3_ = nullptr;
  AsyncWebServer* server_ = nullptr;
  Config config_;
  Snapshot snap_;
};
