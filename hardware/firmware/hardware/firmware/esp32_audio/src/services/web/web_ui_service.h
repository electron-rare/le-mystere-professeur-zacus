#pragma once

#include <Arduino.h>

class Mp3Player;
class WifiService;
class RadioService;
class RadioRuntime;
class StoryControllerV2;
class StoryFsManager;
class AsyncWebServer;
class AsyncWebServerRequest;
class AsyncWebServerResponse;
class AsyncWebSocket;
class DNSServer;

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
  void setStoryContext(StoryControllerV2* story, StoryFsManager* fsManager);
  void setRuntime(RadioRuntime* runtime);
  void update(uint32_t nowMs);
  Snapshot snapshot() const;

 private:
  void setupRoutes();
  bool checkAuth(AsyncWebServerRequest* request);
  void sendJsonStatus(AsyncWebServerRequest* request);
  void sendJsonPlayer(AsyncWebServerRequest* request);
  void sendJsonRadio(AsyncWebServerRequest* request);
  void sendJsonWifi(AsyncWebServerRequest* request);
  void sendJsonRtos(AsyncWebServerRequest* request);
  void setRoute(const char* route);
  void setError(const char* error);
  void sendStoryList(AsyncWebServerRequest* request);
  void sendStoryStatus(AsyncWebServerRequest* request);
  void handleStorySelect(AsyncWebServerRequest* request);
  void handleStoryStart(AsyncWebServerRequest* request);
  void handleStoryPause(AsyncWebServerRequest* request);
  void handleStoryResume(AsyncWebServerRequest* request);
  void handleStorySkip(AsyncWebServerRequest* request);
  void handleStoryValidate(AsyncWebServerRequest* request, const char* body);
  void handleStoryDeploy(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total);
  void handleStorySerial(AsyncWebServerRequest* request, const char* body);
  void sendStoryFsInfo(AsyncWebServerRequest* request);
  void sendAuditLog(AsyncWebServerRequest* request);
  void broadcastStatus(uint32_t nowMs);
  void pushAuditEvent(const char* json);
  void sendJson(AsyncWebServerRequest* request, int code, const String& json);
  void sendError(AsyncWebServerRequest* request, int code, const char* message, const char* details);
  void addCorsHeaders(AsyncWebServerResponse* response);
  void handleOptions(AsyncWebServerRequest* request);
  void updateCaptivePortal(uint32_t nowMs);

  WifiService* wifi_ = nullptr;
  RadioService* radio_ = nullptr;
  Mp3Player* mp3_ = nullptr;
  RadioRuntime* runtime_ = nullptr;
  AsyncWebServer* server_ = nullptr;
  AsyncWebSocket* ws_ = nullptr;
  DNSServer* dns_ = nullptr;
  StoryControllerV2* story_ = nullptr;
  StoryFsManager* storyFs_ = nullptr;
  char selectedScenarioId_[32] = "";
  bool storySelected_ = false;
  uint32_t storyStartedAtMs_ = 0U;
  uint32_t lastStatusPingMs_ = 0U;
  uint32_t lastCaptiveCheckMs_ = 0U;
  char lastStepId_[32] = "";
  bool captiveActive_ = false;
  static constexpr size_t kAuditBufferSize = 50U;
  String auditBuffer_[kAuditBufferSize];
  size_t auditHead_ = 0U;
  size_t auditCount_ = 0U;
  Config config_;
  Snapshot snap_;
};
