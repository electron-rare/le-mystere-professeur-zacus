#pragma once

#include "../core/scenario_def.h"

const AppBindingDef* generatedAppBindingById(const char* id);
uint8_t generatedAppBindingCount();
const char* generatedAppBindingIdAt(uint8_t index);

