#include "default_scenario_v2.h"

#include "../generated/scenarios_gen.h"

const ScenarioDef* storyScenarioV2ById(const char* scenarioId) {
  return generatedScenarioById(scenarioId);
}

const ScenarioDef* storyScenarioV2Default() {
  return generatedScenarioDefault();
}

uint8_t storyScenarioV2Count() {
  return generatedScenarioCount();
}

const char* storyScenarioV2IdAt(uint8_t index) {
  return generatedScenarioIdAt(index);
}
