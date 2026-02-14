#include "screen_link.h"

#include <cstdio>
#include <cstring>

namespace {

uint8_t crc8(const uint8_t* data, size_t len) {
  uint8_t crc = 0x00U;
  for (size_t i = 0; i < len; ++i) {
    crc ^= data[i];
    for (uint8_t bit = 0U; bit < 8U; ++bit) {
      if ((crc & 0x80U) != 0U) {
        crc = static_cast<uint8_t>((crc << 1U) ^ 0x07U);
      } else {
        crc <<= 1U;
      }
    }
  }
  return crc;
}

constexpr uint32_t kTxtMinPeriodMs = 80U;
constexpr uint32_t kTxtKeyframePeriodMs = 3000U;

}  // namespace

ScreenLink::ScreenLink(HardwareSerial& serial,
                       uint8_t txPin,
                       uint32_t baud,
                       uint16_t updatePeriodMs,
                       uint16_t changeMinPeriodMs)
    : serial_(serial),
      txPin_(txPin),
      baud_(baud),
      updatePeriodMs_(updatePeriodMs),
      changeMinPeriodMs_(changeMinPeriodMs) {}

void ScreenLink::begin() {
  serial_.begin(baud_, SERIAL_8N1, -1, txPin_);
}

bool ScreenLink::update(const ScreenFrame& frame, bool forceKeyframe) {
  const bool changed = !hasState_ || frame.laDetected != lastLa_ || frame.mp3Playing != lastMp3_ ||
                       frame.sdReady != lastSd_ || frame.mp3Mode != lastMp3Mode_ || frame.key != lastKey_ ||
                       frame.track != lastTrack_ || frame.trackCount != lastTrackCount_ ||
                       frame.volumePercent != lastVolumePercent_ ||
                       frame.micLevelPercent != lastMicLevelPercent_ ||
                       frame.uLockMode != lastULockMode_ || frame.uLockListening != lastULockListening_ ||
                       frame.uSonFunctional != lastUSonFunctional_ ||
                       frame.tuningOffset != lastTuningOffset_ ||
                       frame.tuningConfidence != lastTuningConfidence_ ||
                       frame.micScopeEnabled != lastMicScopeEnabled_ ||
                       frame.unlockHoldPercent != lastUnlockHoldPercent_ ||
                       frame.startupStage != lastStartupStage_ ||
                       frame.appStage != lastAppStage_ ||
                       frame.uiPage != lastUiPage_ ||
                       frame.uiSource != lastUiSource_ ||
                       frame.uiCursor != lastUiCursor_ ||
                       frame.uiOffset != lastUiOffset_ ||
                       frame.uiCount != lastUiCount_ ||
                       frame.queueCount != lastQueueCount_ ||
                       frame.repeatMode != lastRepeatMode_ ||
                       frame.fxActive != lastFxActive_ ||
                       frame.backendMode != lastBackendMode_ ||
                       frame.scanBusy != lastScanBusy_ ||
                       frame.errorCode != lastErrorCode_;
  const uint32_t elapsedMs = frame.nowMs - lastTxMs_;
  const bool due = elapsedMs >= updatePeriodMs_;
  if (!forceKeyframe && !changed && !due) {
    return false;
  }
  if (!forceKeyframe && hasState_ && !due && elapsedMs < changeMinPeriodMs_) {
    return false;
  }

  char payload[280] = {};
  const int payloadLen = snprintf(payload,
                                  sizeof(payload),
                                  "STAT,%u,%u,%u,%lu,%u,%u,%u,%u,%u,%u,%u,%d,%u,%u,%u,%u,%u,%u,%u,%lu,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u",
                                  frame.laDetected ? 1U : 0U,
                                  frame.mp3Playing ? 1U : 0U,
                                  frame.sdReady ? 1U : 0U,
                                  static_cast<unsigned long>(frame.nowMs),
                                  static_cast<unsigned int>(frame.key),
                                  frame.mp3Mode ? 1U : 0U,
                                  static_cast<unsigned int>(frame.track),
                                  static_cast<unsigned int>(frame.trackCount),
                                  static_cast<unsigned int>(frame.volumePercent),
                                  frame.uLockMode ? 1U : 0U,
                                  frame.uSonFunctional ? 1U : 0U,
                                  static_cast<int>(frame.tuningOffset),
                                  static_cast<unsigned int>(frame.tuningConfidence),
                                  frame.uLockListening ? 1U : 0U,
                                  static_cast<unsigned int>(frame.micLevelPercent),
                                  frame.micScopeEnabled ? 1U : 0U,
                                  static_cast<unsigned int>(frame.unlockHoldPercent),
                                  static_cast<unsigned int>(frame.startupStage),
                                  static_cast<unsigned int>(frame.appStage),
                                  static_cast<unsigned long>(frame.sequence),
                                  static_cast<unsigned int>(frame.uiPage),
                                  static_cast<unsigned int>(frame.repeatMode),
                                  frame.fxActive ? 1U : 0U,
                                  static_cast<unsigned int>(frame.backendMode),
                                  frame.scanBusy ? 1U : 0U,
                                  static_cast<unsigned int>(frame.errorCode),
                                  static_cast<unsigned int>(frame.uiCursor),
                                  static_cast<unsigned int>(frame.uiOffset),
                                  static_cast<unsigned int>(frame.uiCount),
                                  static_cast<unsigned int>(frame.queueCount),
                                  static_cast<unsigned int>(frame.uiSource));
  if (payloadLen <= 0) {
    return false;
  }

  const size_t rawLen = strnlen(payload, sizeof(payload));
  const uint8_t crc = crc8(reinterpret_cast<const uint8_t*>(payload), rawLen);

  char txFrame[304] = {};
  const int len = snprintf(txFrame, sizeof(txFrame), "%s,%02X\n", payload, static_cast<unsigned int>(crc));
  if (len <= 0) {
    return false;
  }

  const int available = serial_.availableForWrite();
  if (available >= 0 && available < len) {
    ++txDropCount_;
    return false;
  }
  serial_.write(reinterpret_cast<const uint8_t*>(txFrame), static_cast<size_t>(len));

  hasState_ = true;
  lastLa_ = frame.laDetected;
  lastMp3_ = frame.mp3Playing;
  lastSd_ = frame.sdReady;
  lastMp3Mode_ = frame.mp3Mode;
  lastULockMode_ = frame.uLockMode;
  lastULockListening_ = frame.uLockListening;
  lastUSonFunctional_ = frame.uSonFunctional;
  lastKey_ = frame.key;
  lastTrack_ = frame.track;
  lastTrackCount_ = frame.trackCount;
  lastVolumePercent_ = frame.volumePercent;
  lastMicLevelPercent_ = frame.micLevelPercent;
  lastTuningOffset_ = frame.tuningOffset;
  lastTuningConfidence_ = frame.tuningConfidence;
  lastMicScopeEnabled_ = frame.micScopeEnabled;
  lastUnlockHoldPercent_ = frame.unlockHoldPercent;
  lastStartupStage_ = frame.startupStage;
  lastAppStage_ = frame.appStage;
  lastUiPage_ = frame.uiPage;
  lastUiSource_ = frame.uiSource;
  lastUiCursor_ = frame.uiCursor;
  lastUiOffset_ = frame.uiOffset;
  lastUiCount_ = frame.uiCount;
  lastQueueCount_ = frame.queueCount;
  lastRepeatMode_ = frame.repeatMode;
  lastFxActive_ = frame.fxActive;
  lastBackendMode_ = frame.backendMode;
  lastScanBusy_ = frame.scanBusy;
  lastErrorCode_ = frame.errorCode;
  lastSequence_ = frame.sequence;
  lastTxMs_ = frame.nowMs;
  ++txFrameCount_;

  if (static_cast<uint32_t>(frame.nowMs - lastTxtTxMs_) < kTxtMinPeriodMs) {
    return true;
  }

  bool keyframeTxt = false;
  if (lastTxtKeyframeMs_ == 0U ||
      static_cast<uint32_t>(frame.nowMs - lastTxtKeyframeMs_) >= kTxtKeyframePeriodMs) {
    keyframeTxt = true;
  }

  ScreenTextSlot candidate = ScreenTextSlot::kNowTitle1;
  bool hasCandidate = false;
  for (uint8_t i = 0U; i < static_cast<uint8_t>(ScreenTextSlot::kCount); ++i) {
    const uint8_t slotIndex = keyframeTxt
                                  ? static_cast<uint8_t>((txtKeyframeCursor_ + i) %
                                                         static_cast<uint8_t>(ScreenTextSlot::kCount))
                                  : i;
    const ScreenTextSlot slot = static_cast<ScreenTextSlot>(slotIndex);
    const char* newText = frame.txtSlots[slotIndex];
    if (newText == nullptr) {
      continue;
    }
    if (keyframeTxt || strncmp(lastTxt_[slotIndex], newText, ScreenFrame::kTextSlotLen) != 0) {
      candidate = slot;
      hasCandidate = true;
      break;
    }
  }

  if (hasCandidate) {
    const uint8_t idx = static_cast<uint8_t>(candidate);
    if (sendTxtSlot(candidate, frame.txtSlots[idx], frame.sequence)) {
      snprintf(lastTxt_[idx], sizeof(lastTxt_[idx]), "%s", frame.txtSlots[idx]);
      lastTxtTxMs_ = frame.nowMs;
      if (keyframeTxt) {
        txtKeyframeCursor_ =
            static_cast<uint8_t>((idx + 1U) % static_cast<uint8_t>(ScreenTextSlot::kCount));
        if (txtKeyframeCursor_ == 0U) {
          lastTxtKeyframeMs_ = frame.nowMs;
        }
      }
    }
  } else if (keyframeTxt) {
    lastTxtKeyframeMs_ = frame.nowMs;
  }

  return true;
}

void ScreenLink::resetStats() {
  txFrameCount_ = 0U;
  txDropCount_ = 0U;
  lastTxMs_ = 0U;
  lastTxtTxMs_ = 0U;
  lastTxtKeyframeMs_ = 0U;
  txtKeyframeCursor_ = 0U;
  for (uint8_t i = 0U; i < static_cast<uint8_t>(ScreenTextSlot::kCount); ++i) {
    lastTxt_[i][0] = '\0';
  }
}

uint32_t ScreenLink::txFrameCount() const {
  return txFrameCount_;
}

uint32_t ScreenLink::txDropCount() const {
  return txDropCount_;
}

uint32_t ScreenLink::lastTxMs() const {
  return lastTxMs_;
}

bool ScreenLink::sendTxtSlot(ScreenTextSlot slot, const char* text, uint32_t seq) {
  char sanitized[ScreenFrame::kTextSlotLen] = {};
  snprintf(sanitized, sizeof(sanitized), "%s", (text != nullptr) ? text : "");
  sanitizeScreenText(sanitized, sizeof(sanitized));

  char payload[140] = {};
  const int payloadLen = snprintf(payload,
                                  sizeof(payload),
                                  "TXT,%lu,%s,%s",
                                  static_cast<unsigned long>(seq),
                                  screenTextSlotToken(slot),
                                  sanitized);
  if (payloadLen <= 0) {
    return false;
  }
  const size_t rawLen = strnlen(payload, sizeof(payload));
  const uint8_t crc = crc8(reinterpret_cast<const uint8_t*>(payload), rawLen);

  char frame[156] = {};
  const int len = snprintf(frame, sizeof(frame), "%s,%02X\n", payload, static_cast<unsigned int>(crc));
  if (len <= 0) {
    return false;
  }

  const int available = serial_.availableForWrite();
  if (available >= 0 && available < len) {
    ++txDropCount_;
    return false;
  }
  serial_.write(reinterpret_cast<const uint8_t*>(frame), static_cast<size_t>(len));
  return true;
}
