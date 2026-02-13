#pragma once

#include <Arduino.h>

class ScreenLink {
 public:
  ScreenLink(HardwareSerial& serial,
             uint8_t txPin,
             uint32_t baud,
             uint16_t updatePeriodMs);

  void begin();
  void update(bool laDetected,
              bool mp3Playing,
              bool sdReady,
              bool mp3Mode,
              bool uLockMode,
              bool uLockListening,
              bool uSonFunctional,
              uint8_t key,
              uint16_t track,
              uint16_t trackCount,
              uint8_t volumePercent,
              uint8_t micLevelPercent,
              int8_t tuningOffset,
              uint8_t tuningConfidence,
              bool micScopeEnabled,
              uint8_t unlockHoldPercent,
              uint8_t startupStage,
              uint8_t appStage,
              uint32_t nowMs);

 private:
  HardwareSerial& serial_;
  uint8_t txPin_;
  uint32_t baud_;
  uint16_t updatePeriodMs_;

  bool hasState_ = false;
  bool lastLa_ = false;
  bool lastMp3_ = false;
  bool lastSd_ = false;
  bool lastMp3Mode_ = false;
  bool lastULockMode_ = false;
  bool lastULockListening_ = false;
  bool lastUSonFunctional_ = false;
  uint8_t lastKey_ = 0;
  uint16_t lastTrack_ = 0;
  uint16_t lastTrackCount_ = 0;
  uint8_t lastVolumePercent_ = 0;
  uint8_t lastMicLevelPercent_ = 0;
  int8_t lastTuningOffset_ = 0;
  uint8_t lastTuningConfidence_ = 0;
  bool lastMicScopeEnabled_ = false;
  uint8_t lastUnlockHoldPercent_ = 0;
  uint8_t lastStartupStage_ = 0;
  uint8_t lastAppStage_ = 0;
  uint32_t lastTxMs_ = 0;
};
