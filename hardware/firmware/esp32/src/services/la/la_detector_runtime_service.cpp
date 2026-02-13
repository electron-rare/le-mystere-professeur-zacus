#include "la_detector_runtime_service.h"

#include <cstring>

namespace {

constexpr uint32_t kMaxTickDeltaMs = 250U;

void copyText(char* out, size_t outLen, const char* in) {
  if (out == nullptr || outLen == 0U) {
    return;
  }
  out[0] = '\0';
  if (in == nullptr || in[0] == '\0') {
    return;
  }
  snprintf(out, outLen, "%s", in);
}

}  // namespace

LaDetectorRuntimeService::LaDetectorRuntimeService(bool (*detectedFn)()) : detectedFn_(detectedFn) {}

void LaDetectorRuntimeService::reset() {
  active_ = false;
  detectionEnabled_ = false;
  listening_ = false;
  uSonFunctional_ = false;
  detected_ = false;
  config_ = {};
  clearProgress(true);
  copyText(stopReason_, sizeof(stopReason_), "RESET");
}

void LaDetectorRuntimeService::setEnvironment(bool detectionEnabled,
                                              bool listening,
                                              bool uSonFunctional) {
  detectionEnabled_ = detectionEnabled;
  listening_ = listening;
  uSonFunctional_ = uSonFunctional;
}

void LaDetectorRuntimeService::start(const Config& config, uint32_t nowMs) {
  config_ = config;
  if (config_.holdMs < 100U) {
    config_.holdMs = 100U;
  }
  if (config_.unlockEventName == nullptr || config_.unlockEventName[0] == '\0') {
    config_.unlockEventName = "UNLOCK";
  }
  active_ = true;
  clearProgress(true);
  lastUpdateMs_ = nowMs;
  copyText(stopReason_, sizeof(stopReason_), "RUNNING");
}

void LaDetectorRuntimeService::stop(const char* reason) {
  active_ = false;
  clearProgress(true);
  copyText(stopReason_, sizeof(stopReason_), reason != nullptr ? reason : "STOPPED");
}

void LaDetectorRuntimeService::update(uint32_t nowMs) {
  if (!active_) {
    detected_ = false;
    return;
  }

  const bool listeningReady = !config_.requireListening || listening_;
  const bool canDetect = detectionEnabled_ && listeningReady && !uSonFunctional_;
  if (!canDetect) {
    clearProgress(false);
    lastUpdateMs_ = nowMs;
    detected_ = false;
    return;
  }

  detected_ = (detectedFn_ != nullptr) ? detectedFn_() : false;
  if (!detected_) {
    clearProgress(false);
    lastUpdateMs_ = nowMs;
    return;
  }

  uint32_t deltaMs = 0U;
  if (lastUpdateMs_ != 0U && nowMs >= lastUpdateMs_) {
    deltaMs = nowMs - lastUpdateMs_;
    if (deltaMs > kMaxTickDeltaMs) {
      deltaMs = kMaxTickDeltaMs;
    }
  }
  lastUpdateMs_ = nowMs;

  if (holdAccumMs_ < config_.holdMs) {
    holdAccumMs_ += deltaMs;
    if (holdAccumMs_ > config_.holdMs) {
      holdAccumMs_ = config_.holdMs;
    }
  }

  if (!unlockLatched_ && holdAccumMs_ >= config_.holdMs) {
    unlockLatched_ = true;
    unlockPending_ = true;
  }
}

bool LaDetectorRuntimeService::consumeUnlock() {
  if (!unlockPending_) {
    return false;
  }
  unlockPending_ = false;
  return true;
}

LaDetectorRuntimeService::Snapshot LaDetectorRuntimeService::snapshot() const {
  Snapshot out = {};
  out.active = active_;
  out.detectionEnabled = detectionEnabled_;
  out.listening = listening_;
  out.uSonFunctional = uSonFunctional_;
  out.detected = detected_;
  out.holdMs = holdAccumMs_;
  out.holdTargetMs = config_.holdMs;
  out.unlockLatched = unlockLatched_;
  out.unlockPending = unlockPending_;
  out.unlockEventName = config_.unlockEventName;
  return out;
}

bool LaDetectorRuntimeService::isActive() const {
  return active_;
}

uint8_t LaDetectorRuntimeService::holdPercent() const {
  if (!active_) {
    return 0U;
  }
  if (config_.holdMs == 0U || holdAccumMs_ >= config_.holdMs) {
    return 100U;
  }
  return static_cast<uint8_t>((holdAccumMs_ * 100U) / config_.holdMs);
}

const char* LaDetectorRuntimeService::lastStopReason() const {
  return stopReason_;
}

void LaDetectorRuntimeService::clearProgress(bool resetLatch) {
  holdAccumMs_ = 0U;
  unlockPending_ = false;
  if (resetLatch) {
    unlockLatched_ = false;
  }
}
