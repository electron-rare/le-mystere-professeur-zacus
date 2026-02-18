#pragma once

#include <Arduino.h>

namespace screen_core {

constexpr uint8_t kStartupStageInactive = 0;
constexpr uint8_t kStartupStageBootValidation = 1;

constexpr uint8_t kAppStageULockWaiting = 0;
constexpr uint8_t kAppStageULockListening = 1;
constexpr uint8_t kAppStageUSonFunctional = 2;
constexpr uint8_t kAppStageMp3 = 3;

struct TelemetryState {
  bool laDetected = false;
  bool mp3Playing = false;
  bool sdReady = false;
  bool mp3Mode = false;
  bool uLockMode = false;
  bool uLockListening = false;
  bool uSonFunctional = false;
  uint32_t uptimeMs = 0;
  uint8_t key = 0;
  uint16_t track = 0;
  uint16_t trackCount = 0;
  uint8_t volumePercent = 0;
  uint8_t micLevelPercent = 0;
  bool micScopeEnabled = false;
  uint8_t unlockHoldPercent = 0;
  uint8_t startupStage = kStartupStageInactive;
  uint8_t appStage = kAppStageULockWaiting;
  uint32_t frameSeq = 0;
  uint8_t uiPage = 0;
  uint16_t uiCursor = 0;
  uint16_t uiOffset = 0;
  uint16_t uiCount = 0;
  uint16_t queueCount = 0;
  uint8_t repeatMode = 0;
  bool fxActive = false;
  uint8_t backendMode = 0;
  bool scanBusy = false;
  uint8_t errorCode = 0;
  int8_t tuningOffset = 0;
  uint8_t tuningConfidence = 0;
  uint32_t lastRxMs = 0;
};

}  // namespace screen_core
