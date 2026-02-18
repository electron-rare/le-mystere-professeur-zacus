#pragma once

#include <Arduino.h>

enum class StoryActionType : uint8_t {
  kNoop = 0,
  kTrace,
  kQueueSonarCue,
  kRequestSdRefresh,
};

struct StoryActionDef {
  const char* id;
  StoryActionType type;
  int32_t value;
};

const StoryActionDef* storyFindAction(const char* actionId);
