#pragma once

#include <Arduino.h>

enum class UiPage : uint8_t {
  kNowPlaying = 0,
  kList = 1,
  kSettings = 2,
};

enum class UiSource : uint8_t {
  kSd = 0,
  kRadio = 1,
};

enum class UiOutCmd : uint8_t {
  kNone = 0,
  kPlayPause,
  kNext,
  kPrev,
  kVolDelta,
  kVolSet,
  kSourceSet,
  kSeek,
  kStationDelta,
  kRequestState,
};

struct UiOutgoingCommand {
  UiOutCmd cmd = UiOutCmd::kNone;
  int32_t value = 0;
  char textValue[16] = {};
};

struct UiRemoteState {
  bool playing = false;
  UiSource source = UiSource::kSd;
  char title[96] = {};
  char artist[64] = {};
  char station[64] = {};
  int32_t posSec = 0;
  int32_t durSec = 0;
  int32_t volume = 0;
  int32_t rssi = -127;
  int32_t bufferPercent = -1;
  char error[64] = {};
};

struct UiRemoteTick {
  int32_t posSec = 0;
  int32_t bufferPercent = -1;
  float vu = 0.0f;
};

struct UiRemoteList {
  UiSource source = UiSource::kSd;
  uint16_t offset = 0;
  uint16_t total = 0;
  uint16_t cursor = 0;
  uint8_t count = 0;
  char items[8][48] = {};
};

inline const char* uiSourceToken(UiSource source) {
  return (source == UiSource::kRadio) ? "radio" : "sd";
}

inline UiSource uiSourceFromToken(const char* token) {
  if (token != nullptr && strcmp(token, "radio") == 0) {
    return UiSource::kRadio;
  }
  return UiSource::kSd;
}

inline const char* uiPageLabel(UiPage page) {
  switch (page) {
    case UiPage::kNowPlaying:
      return "LECTURE";
    case UiPage::kList:
      return "LISTE";
    case UiPage::kSettings:
      return "REGLAGES";
    default:
      return "LECTURE";
  }
}
