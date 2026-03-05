#pragma once

#include <Arduino.h>

struct ScreenSceneDef {
  const char* id;
  uint8_t uiPage;
  uint8_t appStageHint;
};

// Canonical ID normalization is authoritative for runtime wiring.
// Legacy aliases are intentionally kept during the migration window only.
const ScreenSceneDef* storyFindScreenScene(const char* sceneId);
const char* storyNormalizeScreenSceneId(const char* sceneId);
