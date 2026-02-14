#pragma once

#include <Arduino.h>

#include "../screen/screen_frame.h"
#include "ui_link_v2.h"

enum class UiLinkInputType : uint8_t {
  kButton = 0,
  kTouch,
};

struct UiLinkInputEvent {
  UiLinkInputType type = UiLinkInputType::kButton;
  UiBtnId btnId = UI_BTN_UNKNOWN;
  UiBtnAction btnAction = UI_BTN_ACTION_UNKNOWN;
  UiTouchAction touchAction = UI_TOUCH_ACTION_UNKNOWN;
  int16_t x = 0;
  int16_t y = 0;
  uint32_t tsMs = 0;
};

class UiLink {
 public:
  UiLink(HardwareSerial& serial,
         uint8_t rxPin,
         uint8_t txPin,
         uint32_t baud,
         uint16_t updatePeriodMs,
         uint16_t changeMinPeriodMs,
         uint16_t heartbeatMs,
         uint16_t timeoutMs);

  void begin();
  void poll(uint32_t nowMs);

  bool update(const ScreenFrame& frame, bool forceKeyframe = false);
  bool consumeInputEvent(UiLinkInputEvent* event);

  void resetStats();
  uint32_t txFrameCount() const;
  uint32_t txDropCount() const;
  uint32_t lastTxMs() const;
  uint32_t rxFrameCount() const;
  uint32_t parseErrorCount() const;
  uint32_t crcErrorCount() const;
  uint32_t pingTxCount() const;
  uint32_t pongRxCount() const;
  bool connected() const;
  uint32_t lastRxMs() const;

 private:
  bool enqueueInput(const UiLinkInputEvent& event);
  bool handleIncomingFrame(const UiLinkFrame& frame, uint32_t nowMs);
  bool sendAck();
  bool sendPing(uint32_t nowMs);
  bool sendStateFrame(const ScreenFrame& frame, bool keyframe);

  HardwareSerial& serial_;
  uint8_t rxPin_;
  uint8_t txPin_;
  uint32_t baud_;
  uint16_t updatePeriodMs_;
  uint16_t changeMinPeriodMs_;
  uint16_t heartbeatMs_;
  uint16_t timeoutMs_;

  static constexpr uint8_t kInputQueueSize = 16u;
  UiLinkInputEvent inputQueue_[kInputQueueSize] = {};
  uint8_t inputHead_ = 0u;
  uint8_t inputTail_ = 0u;

  char rxLine_[UILINK_V2_MAX_LINE + 1u] = {};
  uint16_t rxLineLen_ = 0u;
  bool dropCurrentLine_ = false;

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
  uint16_t lastUiCursor_ = 0;
  uint16_t lastUiOffset_ = 0;
  uint16_t lastUiCount_ = 0;
  uint16_t lastQueueCount_ = 0;
  uint8_t lastRepeatMode_ = 0;
  bool lastFxActive_ = false;
  uint8_t lastBackendMode_ = 0;
  bool lastScanBusy_ = false;
  uint8_t lastErrorCode_ = 0;

  uint32_t lastTxMs_ = 0u;
  uint32_t lastRxMs_ = 0u;
  uint32_t lastPingMs_ = 0u;
  uint32_t txFrameCount_ = 0u;
  uint32_t txDropCount_ = 0u;
  uint32_t rxFrameCount_ = 0u;
  uint32_t parseErrorCount_ = 0u;
  uint32_t crcErrorCount_ = 0u;
  uint32_t pingTxCount_ = 0u;
  uint32_t pongRxCount_ = 0u;
  uint32_t sessionCounter_ = 0u;

  bool connected_ = false;
  bool ackPending_ = false;
  bool forceKeyframePending_ = false;
};
