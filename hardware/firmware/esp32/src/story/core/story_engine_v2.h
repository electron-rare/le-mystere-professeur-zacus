#pragma once

#include <Arduino.h>

#include "scenario_def.h"
#include "story_events.h"

class StoryEngineV2 {
 public:
  bool loadScenario(const ScenarioDef& scenario);
  bool start(const char* scenarioId, uint32_t nowMs);
  void stop(const char* reason);

  void update(uint32_t nowMs);
  bool postEvent(const StoryEvent& event);
  bool jumpToStep(const char* stepId, const char* reason, uint32_t nowMs);

  StorySnapshot snapshot() const;
  const ScenarioDef* scenario() const;
  const StepDef* currentStep() const;

  bool consumeStepChanged();
  const char* lastError() const;
  uint32_t droppedEvents() const;

 private:
  bool transitionTo(uint8_t nextStepIndex, uint32_t nowMs, const char* reason);
  int8_t selectEventTransition(const StoryEvent& event) const;
  int8_t selectImplicitTransition(uint32_t nowMs) const;
  uint32_t computeNextDueAtMs(uint32_t nowMs) const;

  const ScenarioDef* scenario_ = nullptr;
  StoryEventQueue queue_;
  uint8_t currentStepIndex_ = 0U;
  uint8_t previousStepIndex_ = 0U;
  bool running_ = false;
  bool stepChanged_ = false;
  uint32_t enteredAtMs_ = 0U;
  char lastError_[32] = "OK";
};
