#pragma once

#include <Arduino.h>

#include "screen_frame.h"

class ScreenLink {
 public:
  ScreenLink(HardwareSerial& serial,
             uint8_t txPin,
             uint32_t baud,
             uint16_t updatePeriodMs,
             uint16_t changeMinPeriodMs);

  void begin();
  bool update(const ScreenFrame& frame, bool forceKeyframe = false);
  uint32_t txFrameCount() const;
  uint32_t txDropCount() const;
  uint32_t lastTxMs() const;

 private:
  HardwareSerial& serial_;
  uint8_t txPin_;
  uint32_t baud_;
  uint16_t updatePeriodMs_;
  uint16_t changeMinPeriodMs_;

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
  uint8_t lastUiPage_ = 0;
  uint8_t lastRepeatMode_ = 0;
  bool lastFxActive_ = false;
  uint8_t lastBackendMode_ = 0;
  bool lastScanBusy_ = false;
  uint8_t lastErrorCode_ = 0;
  uint32_t lastSequence_ = 0;
  uint32_t lastTxMs_ = 0;
  uint32_t txFrameCount_ = 0U;
  uint32_t txDropCount_ = 0U;
};
