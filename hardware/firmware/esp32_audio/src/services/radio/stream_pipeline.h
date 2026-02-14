#pragma once

#include <Arduino.h>

class StreamPipeline {
 public:
  enum class State : uint8_t {
    kIdle = 0,
    kConnecting,
    kBuffering,
    kStreaming,
    kRetrying,
    kError,
  };

  struct Snapshot {
    State state = State::kIdle;
    char url[180] = {};
    char codec[12] = "AUTO";
    char title[96] = "";
    uint16_t bitrateKbps = 0U;
    uint8_t bufferPercent = 0U;
    uint32_t lastStateMs = 0U;
    uint32_t retries = 0U;
    char lastError[32] = "OK";
  };

  void begin();
  void update(uint32_t nowMs, bool networkReady);

  bool start(const char* url, const char* codec, const char* reason);
  void stop(const char* reason);

  Snapshot snapshot() const;
  bool isActive() const;

  static const char* stateLabel(State state);

 private:
  void setState(State state, uint32_t nowMs);
  void copyText(char* out, size_t outLen, const char* text);

  Snapshot snap_;
  uint32_t stateSinceMs_ = 0U;
  uint32_t nextRetryMs_ = 0U;
};
