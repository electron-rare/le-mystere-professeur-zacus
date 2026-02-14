#include "stat_parser.h"

#include <cstdlib>
#include <cstring>

namespace screen_core {

namespace {

bool parseUint32Field(const UiLinkFrame& frame, const char* key, uint32_t* outValue) {
  if (outValue == nullptr) {
    return false;
  }
  const UiLinkField* field = uiLinkFindField(&frame, key);
  if (field == nullptr || field->value[0] == '\0') {
    return false;
  }
  char* end = nullptr;
  const unsigned long value = strtoul(field->value, &end, 10);
  if (end == nullptr || *end != '\0') {
    return false;
  }
  *outValue = static_cast<uint32_t>(value);
  return true;
}

bool parseInt32Field(const UiLinkFrame& frame, const char* key, int32_t* outValue) {
  if (outValue == nullptr) {
    return false;
  }
  const UiLinkField* field = uiLinkFindField(&frame, key);
  if (field == nullptr || field->value[0] == '\0') {
    return false;
  }
  char* end = nullptr;
  const long value = strtol(field->value, &end, 10);
  if (end == nullptr || *end != '\0') {
    return false;
  }
  *outValue = static_cast<int32_t>(value);
  return true;
}

bool parseBoolField(const UiLinkFrame& frame, const char* key, bool* outValue) {
  if (outValue == nullptr) {
    return false;
  }
  const UiLinkField* field = uiLinkFindField(&frame, key);
  if (field == nullptr || field->value[0] == '\0') {
    return false;
  }
  if (strcmp(field->value, "1") == 0 || strcmp(field->value, "true") == 0) {
    *outValue = true;
    return true;
  }
  if (strcmp(field->value, "0") == 0 || strcmp(field->value, "false") == 0) {
    *outValue = false;
    return true;
  }
  return false;
}

uint8_t clampU8(int32_t value, uint8_t minValue, uint8_t maxValue) {
  if (value < minValue) {
    return minValue;
  }
  if (value > maxValue) {
    return maxValue;
  }
  return static_cast<uint8_t>(value);
}

}  // namespace

bool parseStatFrame(const UiLinkFrame& frame, TelemetryState* out, uint32_t nowMs) {
  if (out == nullptr) {
    return false;
  }
  if (frame.type != UILINK_MSG_STAT && frame.type != UILINK_MSG_KEYFRAME) {
    return false;
  }

  uint32_t u32 = 0;
  int32_t i32 = 0;
  bool b = false;

  if (parseUint32Field(frame, "seq", &u32)) {
    out->frameSeq = u32;
  }
  if (parseUint32Field(frame, "ms", &u32)) {
    out->uptimeMs = u32;
  }

  if (parseBoolField(frame, "la", &b)) {
    out->laDetected = b;
  }
  if (parseBoolField(frame, "mp3", &b)) {
    out->mp3Playing = b;
  }
  if (parseBoolField(frame, "sd", &b)) {
    out->sdReady = b;
  }
  if (parseBoolField(frame, "u_lock", &b)) {
    out->uLockMode = b;
  }
  if (parseBoolField(frame, "u_lock_listen", &b)) {
    out->uLockListening = b;
  }
  if (parseBoolField(frame, "u_son", &b)) {
    out->uSonFunctional = b;
  }
  if (parseBoolField(frame, "fx", &b)) {
    out->fxActive = b;
  }
  if (parseBoolField(frame, "scan", &b)) {
    out->scanBusy = b;
  }

  if (parseUint32Field(frame, "key", &u32)) {
    out->key = clampU8(static_cast<int32_t>(u32), 0, 6);
  }
  if (parseUint32Field(frame, "track", &u32)) {
    out->track = static_cast<uint16_t>(u32);
  }
  if (parseUint32Field(frame, "track_total", &u32)) {
    out->trackCount = static_cast<uint16_t>(u32);
  }
  if (parseUint32Field(frame, "vol", &u32)) {
    out->volumePercent = clampU8(static_cast<int32_t>(u32), 0, 100);
  }
  if (parseInt32Field(frame, "tune_off", &i32)) {
    if (i32 < -8) {
      i32 = -8;
    }
    if (i32 > 8) {
      i32 = 8;
    }
    out->tuningOffset = static_cast<int8_t>(i32);
  }
  if (parseUint32Field(frame, "tune_conf", &u32)) {
    out->tuningConfidence = clampU8(static_cast<int32_t>(u32), 0, 100);
  }
  if (parseUint32Field(frame, "mic", &u32)) {
    out->micLevelPercent = clampU8(static_cast<int32_t>(u32), 0, 100);
  }
  if (parseUint32Field(frame, "hold", &u32)) {
    out->unlockHoldPercent = clampU8(static_cast<int32_t>(u32), 0, 100);
  }
  if (parseUint32Field(frame, "startup", &u32)) {
    out->startupStage = (u32 == kStartupStageBootValidation) ? kStartupStageBootValidation
                                                             : kStartupStageInactive;
  }
  if (parseUint32Field(frame, "app", &u32)) {
    if (u32 > kAppStageMp3) {
      u32 = kAppStageULockWaiting;
    }
    out->appStage = static_cast<uint8_t>(u32);
  }
  if (parseUint32Field(frame, "ui_page", &u32)) {
    out->uiPage = static_cast<uint8_t>(u32);
  }
  if (parseUint32Field(frame, "ui_cursor", &u32)) {
    out->uiCursor = static_cast<uint16_t>(u32);
  }
  if (parseUint32Field(frame, "ui_offset", &u32)) {
    out->uiOffset = static_cast<uint16_t>(u32);
  }
  if (parseUint32Field(frame, "ui_count", &u32)) {
    out->uiCount = static_cast<uint16_t>(u32);
  }
  if (parseUint32Field(frame, "queue", &u32)) {
    out->queueCount = static_cast<uint16_t>(u32);
  }
  if (parseUint32Field(frame, "repeat", &u32)) {
    out->repeatMode = static_cast<uint8_t>(u32);
  }
  if (parseUint32Field(frame, "backend", &u32)) {
    out->backendMode = static_cast<uint8_t>(u32);
  }
  if (parseUint32Field(frame, "err", &u32)) {
    out->errorCode = static_cast<uint8_t>(u32);
  }

  const UiLinkField* modeField = uiLinkFindField(&frame, "mode");
  if (modeField != nullptr) {
    if (strcmp(modeField->value, "MP3") == 0) {
      out->mp3Mode = true;
      out->appStage = kAppStageMp3;
    } else if (strcmp(modeField->value, "U_LOCK") == 0) {
      out->mp3Mode = false;
      if (!out->uLockMode) {
        out->uLockMode = true;
      }
      if (out->appStage > kAppStageULockListening) {
        out->appStage = kAppStageULockWaiting;
      }
    } else if (strcmp(modeField->value, "STORY") == 0) {
      out->mp3Mode = false;
      out->uSonFunctional = true;
      out->appStage = kAppStageUSonFunctional;
    } else {
      out->mp3Mode = false;
    }
  }

  out->lastRxMs = nowMs;
  return true;
}

}  // namespace screen_core
