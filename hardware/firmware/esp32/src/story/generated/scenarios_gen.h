#pragma once

#include <Arduino.h>

#include "../core/scenario_def.h"

const ScenarioDef* generatedScenarioById(const char* id);
const ScenarioDef* generatedScenarioDefault();
uint8_t generatedScenarioCount();
const char* generatedScenarioIdAt(uint8_t index);

