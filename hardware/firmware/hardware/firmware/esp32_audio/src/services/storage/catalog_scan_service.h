#pragma once

#include <Arduino.h>

class CatalogScanService {
 public:
  enum class State : uint8_t {
    kIdle = 0,
    kRequested,
    kRunning,
    kDone,
    kFailed,
    kCanceled,
  };

  void reset();
  void request(bool forceRebuild = false);
  void start(uint32_t nowMs);
  void finish(State state, uint32_t nowMs);
  void cancel();

  bool isBusy() const;
  bool hasPendingRequest() const;
  State state() const;
  bool forceRebuildRequested() const;
  uint32_t startedAtMs() const;
  uint32_t finishedAtMs() const;

 private:
  State state_ = State::kIdle;
  bool forceRebuildRequested_ = false;
  bool queuedRequest_ = false;
  bool queuedForceRebuild_ = false;
  uint32_t startedAtMs_ = 0U;
  uint32_t finishedAtMs_ = 0U;
};
