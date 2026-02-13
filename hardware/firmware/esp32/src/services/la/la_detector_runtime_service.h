#pragma once

#include <Arduino.h>

class LaDetectorRuntimeService {
 public:
  struct Config {
    uint32_t holdMs = 3000U;
    bool requireListening = true;
    const char* unlockEventName = "UNLOCK";
  };

  struct Snapshot {
    bool active = false;
    bool detectionEnabled = false;
    bool listening = false;
    bool uSonFunctional = false;
    bool detected = false;
    uint32_t holdMs = 0U;
    uint32_t holdTargetMs = 0U;
    bool unlockLatched = false;
    bool unlockPending = false;
    const char* unlockEventName = "UNLOCK";
  };

  explicit LaDetectorRuntimeService(bool (*detectedFn)());

  void reset();
  void setEnvironment(bool detectionEnabled, bool listening, bool uSonFunctional);
  void start(const Config& config, uint32_t nowMs);
  void stop(const char* reason);
  void update(uint32_t nowMs);

  bool consumeUnlock();
  Snapshot snapshot() const;
  bool isActive() const;
  uint8_t holdPercent() const;
  const char* lastStopReason() const;

 private:
  void clearProgress(bool resetLatch);

  bool (*detectedFn_)() = nullptr;
  Config config_ = {};
  bool active_ = false;
  bool detectionEnabled_ = false;
  bool listening_ = false;
  bool uSonFunctional_ = false;
  bool detected_ = false;
  bool unlockLatched_ = false;
  bool unlockPending_ = false;
  uint32_t holdAccumMs_ = 0U;
  uint32_t lastUpdateMs_ = 0U;
  char stopReason_[24] = "IDLE";
};
