#pragma once

#include <Arduino.h>

struct ScreenFrame {
  bool laDetected = false;
  bool mp3Playing = false;
  bool sdReady = false;
  bool mp3Mode = false;
  bool uLockMode = false;
  bool uLockListening = false;
  bool uSonFunctional = false;
  uint8_t key = 0;
  uint16_t track = 0;
  uint16_t trackCount = 0;
  uint8_t volumePercent = 0;
  uint8_t micLevelPercent = 0;
  int8_t tuningOffset = 0;
  uint8_t tuningConfidence = 0;
  bool micScopeEnabled = false;
  uint8_t unlockHoldPercent = 0;
  uint8_t startupStage = 0;
  uint8_t appStage = 0;
  uint8_t uiPage = 0;
  uint8_t repeatMode = 0;
  bool fxActive = false;
  uint8_t backendMode = 0;
  bool scanBusy = false;
  uint8_t errorCode = 0;
  uint32_t sequence = 0;
  uint32_t nowMs = 0;
};
