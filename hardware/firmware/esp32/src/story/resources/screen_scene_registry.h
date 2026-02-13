#pragma once

#include <Arduino.h>

struct ScreenSceneDef {
  const char* id;
  uint8_t uiPage;
  uint8_t appStageHint;
};

const ScreenSceneDef* storyFindScreenScene(const char* sceneId);
