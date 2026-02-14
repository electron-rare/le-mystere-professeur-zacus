#pragma once

#include <Arduino.h>

enum class UiSerialAction : uint8_t {
  kUnknown = 0,
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

struct UiSerialCommand {
  UiSerialAction action = UiSerialAction::kUnknown;
  bool hasIntValue = false;
  int32_t intValue = 0;
  bool hasTextValue = false;
  char textValue[20] = {};
};

struct UiSerialState {
  bool playing = false;
  const char* source = "sd";
  const char* title = "";
  const char* artist = "";
  const char* station = "";
  int32_t pos = 0;
  int32_t dur = 0;
  int32_t vol = 0;
  int32_t rssi = -127;
  int32_t buffer = -1;
  const char* error = "";
};

struct UiSerialTick {
  int32_t pos = 0;
  int32_t buffer = -1;
  float vu = 0.0f;
};

struct UiSerialList {
  const char* source = "sd";
  uint16_t offset = 0;
  uint16_t total = 0;
  uint16_t cursor = 0;
  uint8_t count = 0;
  const char* items[8] = {};
};
