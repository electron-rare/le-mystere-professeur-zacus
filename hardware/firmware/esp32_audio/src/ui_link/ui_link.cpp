#include "ui_link.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

bool parseUint32(const char* text, uint32_t* outValue) {
  if (text == nullptr || outValue == nullptr || text[0] == '\0') {
    return false;
  }
  char* end = nullptr;
  const unsigned long parsed = strtoul(text, &end, 10);
  if (end == nullptr || *end != '\0') {
    return false;
  }
  *outValue = static_cast<uint32_t>(parsed);
  return true;
}

bool parseInt32(const char* text, int32_t* outValue) {
  if (text == nullptr || outValue == nullptr || text[0] == '\0') {
    return false;
  }
  char* end = nullptr;
  const long parsed = strtol(text, &end, 10);
  if (end == nullptr || *end != '\0') {
    return false;
  }
  *outValue = static_cast<int32_t>(parsed);
  return true;
}

const char* modeToken(const ScreenFrame& frame) {
  if (frame.mp3Mode) {
    return "MP3";
  }
  if (frame.uLockMode) {
    return "U_LOCK";
  }
  if (frame.uSonFunctional) {
    return "STORY";
  }
  return "SIGNAL";
}

}  // namespace

UiLink::UiLink(HardwareSerial& serial,
               uint8_t rxPin,
               uint8_t txPin,
               uint32_t baud,
               uint16_t updatePeriodMs,
               uint16_t changeMinPeriodMs,
               uint16_t heartbeatMs,
               uint16_t timeoutMs)
    : serial_(serial),
      rxPin_(rxPin),
      txPin_(txPin),
      baud_(baud),
      updatePeriodMs_(updatePeriodMs),
      changeMinPeriodMs_(changeMinPeriodMs),
      heartbeatMs_(heartbeatMs),
      timeoutMs_(timeoutMs) {}

void UiLink::begin() {
  serial_.begin(baud_, SERIAL_8N1, rxPin_, txPin_);
}

bool UiLink::enqueueInput(const UiLinkInputEvent& event) {
  const uint8_t next = static_cast<uint8_t>((inputHead_ + 1u) % kInputQueueSize);
  if (next == inputTail_) {
    return false;
  }
  inputQueue_[inputHead_] = event;
  inputHead_ = next;
  return true;
}

bool UiLink::consumeInputEvent(UiLinkInputEvent* event) {
  if (event == nullptr || inputTail_ == inputHead_) {
    return false;
  }
  *event = inputQueue_[inputTail_];
  inputTail_ = static_cast<uint8_t>((inputTail_ + 1u) % kInputQueueSize);
  return true;
}

bool UiLink::handleIncomingFrame(const UiLinkFrame& frame, uint32_t nowMs) {
  ++rxFrameCount_;

  switch (frame.type) {
    case UILINK_MSG_HELLO: {
      const UiLinkField* proto = uiLinkFindField(&frame, "proto");
      if (proto == nullptr || strcmp(proto->value, "2") != 0) {
        return false;
      }
      connected_ = true;
      lastRxMs_ = nowMs;
      ++sessionCounter_;
      ackPending_ = true;
      forceKeyframePending_ = true;
      return true;
    }
    case UILINK_MSG_PONG:
      connected_ = true;
      lastRxMs_ = nowMs;
      ++pongRxCount_;
      return true;
    case UILINK_MSG_BTN: {
      connected_ = true;
      lastRxMs_ = nowMs;

      const UiLinkField* idField = uiLinkFindField(&frame, "id");
      const UiLinkField* actionField = uiLinkFindField(&frame, "action");
      if (idField == nullptr || actionField == nullptr) {
        return false;
      }

      UiLinkInputEvent event = {};
      event.type = UiLinkInputType::kButton;
      event.btnId = uiBtnIdFromToken(idField->value);
      event.btnAction = uiBtnActionFromToken(actionField->value);
      if (event.btnId == UI_BTN_UNKNOWN || event.btnAction == UI_BTN_ACTION_UNKNOWN) {
        return false;
      }

      const UiLinkField* tsField = uiLinkFindField(&frame, "ts");
      uint32_t ts = nowMs;
      if (tsField != nullptr) {
        parseUint32(tsField->value, &ts);
      }
      event.tsMs = ts;
      return enqueueInput(event);
    }
    case UILINK_MSG_TOUCH: {
      connected_ = true;
      lastRxMs_ = nowMs;

      const UiLinkField* xField = uiLinkFindField(&frame, "x");
      const UiLinkField* yField = uiLinkFindField(&frame, "y");
      const UiLinkField* actionField = uiLinkFindField(&frame, "action");
      if (xField == nullptr || yField == nullptr || actionField == nullptr) {
        return false;
      }

      int32_t x = 0;
      int32_t y = 0;
      if (!parseInt32(xField->value, &x) || !parseInt32(yField->value, &y)) {
        return false;
      }

      UiLinkInputEvent event = {};
      event.type = UiLinkInputType::kTouch;
      event.touchAction = uiTouchActionFromToken(actionField->value);
      event.x = static_cast<int16_t>(x);
      event.y = static_cast<int16_t>(y);

      const UiLinkField* tsField = uiLinkFindField(&frame, "ts");
      uint32_t ts = nowMs;
      if (tsField != nullptr) {
        parseUint32(tsField->value, &ts);
      }
      event.tsMs = ts;
      return enqueueInput(event);
    }
    case UILINK_MSG_CMD: {
      connected_ = true;
      lastRxMs_ = nowMs;
      const UiLinkField* op = uiLinkFindField(&frame, "op");
      if (op != nullptr && strcmp(op->value, "request_keyframe") == 0) {
        forceKeyframePending_ = true;
      }
      return true;
    }
    case UILINK_MSG_CAPS:
    case UILINK_MSG_PING:
    case UILINK_MSG_ACK:
    case UILINK_MSG_STAT:
    case UILINK_MSG_KEYFRAME:
    case UILINK_MSG_UNKNOWN:
    default:
      return false;
  }
}

bool UiLink::sendAck() {
  UiLinkField fields[3] = {};
  snprintf(fields[0].key, sizeof(fields[0].key), "proto");
  snprintf(fields[0].value, sizeof(fields[0].value), "%u", static_cast<unsigned int>(UILINK_V2_PROTO));
  snprintf(fields[1].key, sizeof(fields[1].key), "session");
  snprintf(fields[1].value, sizeof(fields[1].value), "%lu", static_cast<unsigned long>(sessionCounter_));

  char line[UILINK_V2_MAX_LINE + 1u] = {};
  const size_t lineLen = uiLinkBuildLine(line, sizeof(line), "ACK", fields, 2u);
  if (lineLen == 0u) {
    return false;
  }

  const int available = serial_.availableForWrite();
  if (available >= 0 && static_cast<size_t>(available) < lineLen) {
    ++txDropCount_;
    return false;
  }
  serial_.write(reinterpret_cast<const uint8_t*>(line), lineLen);
  lastTxMs_ = millis();
  ++txFrameCount_;
  return true;
}

bool UiLink::sendPing(uint32_t nowMs) {
  UiLinkField fields[1] = {};
  snprintf(fields[0].key, sizeof(fields[0].key), "ms");
  snprintf(fields[0].value, sizeof(fields[0].value), "%lu", static_cast<unsigned long>(nowMs));

  char line[UILINK_V2_MAX_LINE + 1u] = {};
  const size_t lineLen = uiLinkBuildLine(line, sizeof(line), "PING", fields, 1u);
  if (lineLen == 0u) {
    return false;
  }

  const int available = serial_.availableForWrite();
  if (available >= 0 && static_cast<size_t>(available) < lineLen) {
    ++txDropCount_;
    return false;
  }
  serial_.write(reinterpret_cast<const uint8_t*>(line), lineLen);
  lastTxMs_ = nowMs;
  ++txFrameCount_;
  ++pingTxCount_;
  return true;
}

bool UiLink::sendStateFrame(const ScreenFrame& frame, bool keyframe) {
  UiLinkField fields[UILINK_V2_MAX_FIELDS] = {};
  uint8_t count = 0u;

  auto addText = [&](const char* key, const char* value) {
    if (count >= UILINK_V2_MAX_FIELDS || key == nullptr || value == nullptr) {
      return false;
    }
    snprintf(fields[count].key, sizeof(fields[count].key), "%s", key);
    snprintf(fields[count].value, sizeof(fields[count].value), "%s", value);
    ++count;
    return true;
  };

  auto addUInt = [&](const char* key, uint32_t value) {
    char buffer[24] = {};
    snprintf(buffer, sizeof(buffer), "%lu", static_cast<unsigned long>(value));
    return addText(key, buffer);
  };

  auto addInt = [&](const char* key, int32_t value) {
    char buffer[24] = {};
    snprintf(buffer, sizeof(buffer), "%ld", static_cast<long>(value));
    return addText(key, buffer);
  };

  const bool ok = addUInt("seq", frame.sequence) && addUInt("ms", frame.nowMs) &&
                  addText("mode", modeToken(frame)) && addUInt("la", frame.laDetected ? 1u : 0u) &&
                  addUInt("mp3", frame.mp3Playing ? 1u : 0u) &&
                  addUInt("sd", frame.sdReady ? 1u : 0u) && addUInt("key", frame.key) &&
                  addUInt("track", frame.track) && addUInt("track_total", frame.trackCount) &&
                  addUInt("vol", frame.volumePercent) &&
                  addUInt("u_lock", frame.uLockMode ? 1u : 0u) &&
                  addUInt("u_son", frame.uSonFunctional ? 1u : 0u) &&
                  addInt("tune_off", frame.tuningOffset) &&
                  addUInt("tune_conf", frame.tuningConfidence) &&
                  addUInt("u_lock_listen", frame.uLockListening ? 1u : 0u) &&
                  addUInt("mic", frame.micLevelPercent) && addUInt("hold", frame.unlockHoldPercent) &&
                  addUInt("startup", frame.startupStage) && addUInt("app", frame.appStage) &&
                  addUInt("ui_page", frame.uiPage) && addUInt("repeat", frame.repeatMode) &&
                  addUInt("fx", frame.fxActive ? 1u : 0u) && addUInt("backend", frame.backendMode) &&
                  addUInt("scan", frame.scanBusy ? 1u : 0u) && addUInt("err", frame.errorCode) &&
                  addUInt("ui_cursor", frame.uiCursor) && addUInt("ui_offset", frame.uiOffset) &&
                  addUInt("ui_count", frame.uiCount) && addUInt("queue", frame.queueCount);
  if (!ok) {
    return false;
  }

  char line[UILINK_V2_MAX_LINE + 1u] = {};
  const char* type = keyframe ? "KEYFRAME" : "STAT";
  const size_t lineLen = uiLinkBuildLine(line, sizeof(line), type, fields, count);
  if (lineLen == 0u) {
    return false;
  }

  const int available = serial_.availableForWrite();
  if (available >= 0 && static_cast<size_t>(available) < lineLen) {
    ++txDropCount_;
    return false;
  }
  serial_.write(reinterpret_cast<const uint8_t*>(line), lineLen);
  lastTxMs_ = frame.nowMs;
  ++txFrameCount_;
  return true;
}

void UiLink::poll(uint32_t nowMs) {
  while (serial_.available() > 0) {
    const int raw = serial_.read();
    if (raw < 0) {
      break;
    }

    const char c = static_cast<char>(raw);
    if (c == '\r') {
      continue;
    }
    if (c == '\n') {
      if (!dropCurrentLine_ && rxLineLen_ > 0u) {
        rxLine_[rxLineLen_] = '\0';
        UiLinkFrame frame = {};
        if (uiLinkParseLine(rxLine_, &frame)) {
          if (!handleIncomingFrame(frame, nowMs)) {
            ++parseErrorCount_;
          }
        } else {
          if (strchr(rxLine_, '*') != nullptr) {
            ++crcErrorCount_;
          } else {
            ++parseErrorCount_;
          }
        }
      }
      rxLineLen_ = 0u;
      dropCurrentLine_ = false;
      continue;
    }

    if (dropCurrentLine_) {
      continue;
    }
    if (rxLineLen_ >= UILINK_V2_MAX_LINE) {
      rxLineLen_ = 0u;
      dropCurrentLine_ = true;
      continue;
    }
    rxLine_[rxLineLen_++] = c;
  }

  if (ackPending_) {
    if (sendAck()) {
      ackPending_ = false;
    }
  }

  if (connected_ && (lastPingMs_ == 0u || static_cast<uint32_t>(nowMs - lastPingMs_) >= heartbeatMs_)) {
    if (sendPing(nowMs)) {
      lastPingMs_ = nowMs;
    }
  }

  if (connected_ && timeoutMs_ > 0u && static_cast<uint32_t>(nowMs - lastRxMs_) > timeoutMs_) {
    connected_ = false;
  }
}

bool UiLink::update(const ScreenFrame& frame, bool forceKeyframe) {
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
                       frame.startupStage != lastStartupStage_ || frame.appStage != lastAppStage_ ||
                       frame.uiPage != lastUiPage_ || frame.uiCursor != lastUiCursor_ ||
                       frame.uiOffset != lastUiOffset_ || frame.uiCount != lastUiCount_ ||
                       frame.queueCount != lastQueueCount_ || frame.repeatMode != lastRepeatMode_ ||
                       frame.fxActive != lastFxActive_ || frame.backendMode != lastBackendMode_ ||
                       frame.scanBusy != lastScanBusy_ || frame.errorCode != lastErrorCode_;

  const uint32_t elapsedMs = frame.nowMs - lastTxMs_;
  const bool due = elapsedMs >= updatePeriodMs_;
  bool keyframe = forceKeyframe || forceKeyframePending_;

  if (!keyframe && !changed && !due) {
    return false;
  }
  if (!keyframe && hasState_ && !due && elapsedMs < changeMinPeriodMs_) {
    return false;
  }

  if (!sendStateFrame(frame, keyframe)) {
    return false;
  }

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
  lastUiCursor_ = frame.uiCursor;
  lastUiOffset_ = frame.uiOffset;
  lastUiCount_ = frame.uiCount;
  lastQueueCount_ = frame.queueCount;
  lastRepeatMode_ = frame.repeatMode;
  lastFxActive_ = frame.fxActive;
  lastBackendMode_ = frame.backendMode;
  lastScanBusy_ = frame.scanBusy;
  lastErrorCode_ = frame.errorCode;

  if (keyframe) {
    forceKeyframePending_ = false;
  }
  return true;
}

void UiLink::resetStats() {
  txFrameCount_ = 0u;
  txDropCount_ = 0u;
  rxFrameCount_ = 0u;
  parseErrorCount_ = 0u;
  crcErrorCount_ = 0u;
  pingTxCount_ = 0u;
  pongRxCount_ = 0u;
  lastTxMs_ = 0u;
  lastRxMs_ = 0u;
  lastPingMs_ = 0u;
}

uint32_t UiLink::txFrameCount() const {
  return txFrameCount_;
}

uint32_t UiLink::txDropCount() const {
  return txDropCount_;
}

uint32_t UiLink::lastTxMs() const {
  return lastTxMs_;
}

uint32_t UiLink::rxFrameCount() const {
  return rxFrameCount_;
}

uint32_t UiLink::parseErrorCount() const {
  return parseErrorCount_;
}

uint32_t UiLink::crcErrorCount() const {
  return crcErrorCount_;
}

uint32_t UiLink::pingTxCount() const {
  return pingTxCount_;
}

uint32_t UiLink::pongRxCount() const {
  return pongRxCount_;
}

bool UiLink::connected() const {
  return connected_;
}

uint32_t UiLink::lastRxMs() const {
  return lastRxMs_;
}

bool UiLink::ackPending() const {
  return ackPending_;
}

uint32_t UiLink::lastPingMs() const {
  return lastPingMs_;
}

uint32_t UiLink::sessionCounter() const {
  return sessionCounter_;
}
