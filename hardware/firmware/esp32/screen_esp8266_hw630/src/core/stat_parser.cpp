#include "stat_parser.h"

#include <cstdio>
#include <cstring>

namespace screen_core {

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

}  // namespace

bool parseStatFrame(const char* frame,
                    TelemetryState* out,
                    uint32_t nowMs,
                    uint32_t* crcErrorCount) {
  if (frame == nullptr || out == nullptr || strncmp(frame, "STAT,", 5) != 0) {
    return false;
  }

  unsigned int la = 0;
  unsigned int mp3 = 0;
  unsigned int sd = 0;
  unsigned long up = 0;
  unsigned int key = 0;
  unsigned int mode = 0;
  unsigned int track = 0;
  unsigned int trackCount = 0;
  unsigned int volumePercent = 0;
  unsigned int uLockMode = 0;
  unsigned int uSonFunctional = 0;
  int tuningOffset = 0;
  unsigned int tuningConfidence = 0;
  unsigned int uLockListening = 0;
  unsigned int micLevelPercent = 0;
  unsigned int micScopeEnabled = 0;
  unsigned int unlockHoldPercent = 0;
  unsigned int startupStage = 0;
  unsigned int appStage = 0;
  unsigned long frameSeq = 0;
  unsigned int uiPage = 0;
  unsigned int repeatMode = 0;
  unsigned int fxActive = 0;
  unsigned int backendMode = 0;
  unsigned int scanBusy = 0;
  unsigned int errorCode = 0;
  unsigned int uiCursor = 0;
  unsigned int uiOffset = 0;
  unsigned int uiCount = 0;
  unsigned int queueCount = 0;
  unsigned int frameCrc = 0;

  const int parsed = sscanf(frame,
                            "STAT,%u,%u,%u,%lu,%u,%u,%u,%u,%u,%u,%u,%d,%u,%u,%u,%u,%u,%u,%u,%lu,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%x",
                            &la,
                            &mp3,
                            &sd,
                            &up,
                            &key,
                            &mode,
                            &track,
                            &trackCount,
                            &volumePercent,
                            &uLockMode,
                            &uSonFunctional,
                            &tuningOffset,
                            &tuningConfidence,
                            &uLockListening,
                            &micLevelPercent,
                            &micScopeEnabled,
                            &unlockHoldPercent,
                            &startupStage,
                            &appStage,
                            &frameSeq,
                            &uiPage,
                            &repeatMode,
                            &fxActive,
                            &backendMode,
                            &scanBusy,
                            &errorCode,
                            &uiCursor,
                            &uiOffset,
                            &uiCount,
                            &queueCount,
                            &frameCrc);
  if (parsed < 19) {
    return false;
  }
  if (parsed >= 27) {
    const char* lastComma = strrchr(frame, ',');
    if (lastComma == nullptr) {
      return false;
    }
    const size_t payloadLen = static_cast<size_t>(lastComma - frame);
    const uint8_t computed = crc8(reinterpret_cast<const uint8_t*>(frame), payloadLen);
    const uint8_t expected = static_cast<uint8_t>(frameCrc & 0xFFU);
    if (computed != expected) {
      if (crcErrorCount != nullptr) {
        ++(*crcErrorCount);
      }
      return false;
    }
  }

  out->laDetected = (la != 0U);
  out->mp3Playing = (mp3 != 0U);
  out->sdReady = (sd != 0U);
  out->uptimeMs = static_cast<uint32_t>(up);
  out->key = static_cast<uint8_t>(key);
  out->mp3Mode = (parsed >= 6) ? (mode != 0U) : false;
  out->track = (parsed >= 7) ? static_cast<uint16_t>(track) : 0;
  out->trackCount = (parsed >= 8) ? static_cast<uint16_t>(trackCount) : 0;
  out->volumePercent = (parsed >= 9) ? static_cast<uint8_t>(volumePercent) : 0;
  out->uLockMode = (parsed >= 10) ? (uLockMode != 0U) : false;
  out->uSonFunctional = (parsed >= 11) ? (uSonFunctional != 0U) : false;

  if (parsed >= 12) {
    if (tuningOffset < -8) {
      tuningOffset = -8;
    } else if (tuningOffset > 8) {
      tuningOffset = 8;
    }
    out->tuningOffset = static_cast<int8_t>(tuningOffset);
  } else {
    out->tuningOffset = 0;
  }

  if (parsed >= 13) {
    if (tuningConfidence > 100U) {
      tuningConfidence = 100U;
    }
    out->tuningConfidence = static_cast<uint8_t>(tuningConfidence);
  } else {
    out->tuningConfidence = 0;
  }

  out->uLockListening = (parsed >= 14) ? (uLockListening != 0U) : false;
  if (parsed >= 15) {
    if (micLevelPercent > 100U) {
      micLevelPercent = 100U;
    }
    out->micLevelPercent = static_cast<uint8_t>(micLevelPercent);
  } else {
    out->micLevelPercent = 0;
  }
  out->micScopeEnabled = (parsed >= 16) ? (micScopeEnabled != 0U) : false;
  if (parsed >= 17) {
    if (unlockHoldPercent > 100U) {
      unlockHoldPercent = 100U;
    }
    out->unlockHoldPercent = static_cast<uint8_t>(unlockHoldPercent);
  } else {
    out->unlockHoldPercent = 0;
  }

  if (parsed >= 18) {
    out->startupStage =
        (startupStage == kStartupStageBootValidation) ? kStartupStageBootValidation
                                                       : kStartupStageInactive;
  } else {
    out->startupStage = kStartupStageInactive;
  }

  if (parsed >= 19) {
    if (appStage > kAppStageMp3) {
      appStage = kAppStageULockWaiting;
    }
    out->appStage = static_cast<uint8_t>(appStage);
  } else if (out->mp3Mode) {
    out->appStage = kAppStageMp3;
  } else if (out->uSonFunctional) {
    out->appStage = kAppStageUSonFunctional;
  } else if (out->uLockMode && out->uLockListening) {
    out->appStage = kAppStageULockListening;
  } else {
    out->appStage = kAppStageULockWaiting;
  }

  if (parsed >= 20) {
    out->frameSeq = static_cast<uint32_t>(frameSeq);
  }
  out->uiPage = (parsed >= 21) ? static_cast<uint8_t>(uiPage) : 0U;
  out->repeatMode = (parsed >= 22) ? static_cast<uint8_t>(repeatMode) : 0U;
  out->fxActive = (parsed >= 23) ? (fxActive != 0U) : false;
  out->backendMode = (parsed >= 24) ? static_cast<uint8_t>(backendMode) : 0U;
  out->scanBusy = (parsed >= 25) ? (scanBusy != 0U) : false;
  out->errorCode = (parsed >= 26) ? static_cast<uint8_t>(errorCode) : 0U;
  out->uiCursor = (parsed >= 27) ? static_cast<uint16_t>(uiCursor) : 0U;
  out->uiOffset = (parsed >= 28) ? static_cast<uint16_t>(uiOffset) : 0U;
  out->uiCount = (parsed >= 29) ? static_cast<uint16_t>(uiCount) : 0U;
  out->queueCount = (parsed >= 30) ? static_cast<uint16_t>(queueCount) : 0U;

  out->lastRxMs = nowMs;
  return true;
}

}  // namespace screen_core
