#pragma once

#include "../core/scenario_def.h"

const ScenarioDef* storyScenarioV2ById(const char* scenarioId);
const ScenarioDef* storyScenarioV2Default();
uint8_t storyScenarioV2Count();
const char* storyScenarioV2IdAt(uint8_t index);
