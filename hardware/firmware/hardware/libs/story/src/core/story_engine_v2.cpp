#include "story_engine_v2.h"

#include <cstring>
#include <cstdio>

namespace {

constexpr uint8_t kEventProcessBudgetPerUpdate = 6U;

bool sameText(const char* lhs, const char* rhs) {
  if (lhs == nullptr || rhs == nullptr) {
    return false;
  }
  return strcmp(lhs, rhs) == 0;
}

bool eventNameMatch(const char* transitionEventName, const char* eventName) {
  if (transitionEventName == nullptr || transitionEventName[0] == '\0') {
    return true;
  }
  if (eventName == nullptr || eventName[0] == '\0') {
    return false;
  }
  return strcmp(transitionEventName, eventName) == 0;
}

}  // namespace

bool StoryEngineV2::loadScenario(const ScenarioDef& scenario) {
  StoryValidationError error;
  if (!storyValidateScenarioDef(scenario, &error)) {
    snprintf(lastError_, sizeof(lastError_), "%s", error.code);
    Serial.printf("[STORY_V2] loadScenario failed code=%s detail=%s\n",
                  error.code != nullptr ? error.code : "-",
                  error.detail != nullptr ? error.detail : "-");
    return false;
  }

  scenario_ = &scenario;
  queue_.clear();
  running_ = false;
  stepChanged_ = false;
  enteredAtMs_ = 0U;
  currentStepIndex_ = 0U;
  previousStepIndex_ = 0U;
  snprintf(lastError_, sizeof(lastError_), "%s", "OK");
  Serial.printf("[STORY_V2] scenario loaded id=%s v=%u\n",
                scenario.id,
                static_cast<unsigned int>(scenario.version));
  return true;
}

bool StoryEngineV2::start(const char* scenarioId, uint32_t nowMs) {
  if (scenario_ == nullptr) {
    snprintf(lastError_, sizeof(lastError_), "%s", "SCENARIO_NOT_LOADED");
    return false;
  }
  if (scenarioId != nullptr && scenarioId[0] != '\0' && !sameText(scenario_->id, scenarioId)) {
    snprintf(lastError_, sizeof(lastError_), "%s", "SCENARIO_ID_MISMATCH");
    return false;
  }

  const int8_t idx = storyFindStepIndex(*scenario_, scenario_->initialStepId);
  if (idx < 0) {
    snprintf(lastError_, sizeof(lastError_), "%s", "INITIAL_STEP_NOT_FOUND");
    return false;
  }

  queue_.clear();
  running_ = true;
  previousStepIndex_ = static_cast<uint8_t>(idx);
  currentStepIndex_ = static_cast<uint8_t>(idx);
  enteredAtMs_ = nowMs;
  stepChanged_ = true;
  snprintf(lastError_, sizeof(lastError_), "%s", "OK");
  Serial.printf("[STORY_V2] start scenario=%s step=%s\n",
                scenario_->id,
                scenario_->steps[currentStepIndex_].id);
  return true;
}

void StoryEngineV2::stop(const char* reason) {
  if (!running_) {
    return;
  }
  running_ = false;
  queue_.clear();
  stepChanged_ = false;
  Serial.printf("[STORY_V2] stop reason=%s\n", reason != nullptr ? reason : "-");
}

bool StoryEngineV2::postEvent(const StoryEvent& event) {
  if (!running_) {
    return false;
  }
  if (!queue_.push(event)) {
    snprintf(lastError_, sizeof(lastError_), "%s", "EVENT_QUEUE_FULL");
    Serial.printf("[STORY_V2] event drop type=%u name=%s\n",
                  static_cast<unsigned int>(event.type),
                  event.name);
    return false;
  }
  return true;
}

void StoryEngineV2::update(uint32_t nowMs) {
  if (!running_ || scenario_ == nullptr) {
    return;
  }

  StoryEvent event;
  uint8_t processed = 0U;
  while (processed < kEventProcessBudgetPerUpdate && queue_.pop(&event)) {
    ++processed;
    const int8_t transitionIndex = selectEventTransition(event);
    if (transitionIndex < 0) {
      continue;
    }

    const StepDef& step = scenario_->steps[currentStepIndex_];
    const TransitionDef& transition = step.transitions[transitionIndex];
    const int8_t targetStepIndex = storyFindStepIndex(*scenario_, transition.targetStepId);
    if (targetStepIndex < 0) {
      snprintf(lastError_, sizeof(lastError_), "%s", "TARGET_STEP_NOT_FOUND");
      continue;
    }
    transitionTo(static_cast<uint8_t>(targetStepIndex), nowMs, transition.id);
    return;
  }

  if (processed >= kEventProcessBudgetPerUpdate && queue_.size() > 0U) {
    snprintf(lastError_, sizeof(lastError_), "%s", "EVENT_BUDGET");
    return;
  }

  const int8_t implicitIndex = selectImplicitTransition(nowMs);
  if (implicitIndex < 0) {
    return;
  }

  const StepDef& step = scenario_->steps[currentStepIndex_];
  const TransitionDef& transition = step.transitions[implicitIndex];
  const int8_t targetStepIndex = storyFindStepIndex(*scenario_, transition.targetStepId);
  if (targetStepIndex < 0) {
    snprintf(lastError_, sizeof(lastError_), "%s", "TARGET_STEP_NOT_FOUND");
    return;
  }
  transitionTo(static_cast<uint8_t>(targetStepIndex), nowMs, transition.id);
}

bool StoryEngineV2::jumpToStep(const char* stepId, const char* reason, uint32_t nowMs) {
  if (!running_ || scenario_ == nullptr) {
    return false;
  }
  const int8_t targetStepIndex = storyFindStepIndex(*scenario_, stepId);
  if (targetStepIndex < 0) {
    snprintf(lastError_, sizeof(lastError_), "%s", "STEP_NOT_FOUND");
    return false;
  }
  return transitionTo(static_cast<uint8_t>(targetStepIndex), nowMs, reason);
}

StorySnapshot StoryEngineV2::snapshot() const {
  StorySnapshot out = {};
  out.mp3GateOpen = true;
  out.running = running_;
  out.queuedEvents = queue_.size();
  if (!running_ || scenario_ == nullptr) {
    return out;
  }

  const StepDef& step = scenario_->steps[currentStepIndex_];
  out.mp3GateOpen = step.mp3GateOpen;
  out.scenarioId = scenario_->id;
  out.stepId = step.id;
  out.previousStepId = scenario_->steps[previousStepIndex_].id;
  out.stepIndex = currentStepIndex_;
  out.enteredAtMs = enteredAtMs_;
  out.nextDueAtMs = computeNextDueAtMs(enteredAtMs_);
  return out;
}

const ScenarioDef* StoryEngineV2::scenario() const {
  return scenario_;
}

const StepDef* StoryEngineV2::currentStep() const {
  if (!running_ || scenario_ == nullptr || currentStepIndex_ >= scenario_->stepCount) {
    return nullptr;
  }
  return &scenario_->steps[currentStepIndex_];
}

bool StoryEngineV2::consumeStepChanged() {
  const bool changed = stepChanged_;
  stepChanged_ = false;
  return changed;
}

const char* StoryEngineV2::lastTransitionId() const {
  return lastTransitionId_[0] != '\0' ? lastTransitionId_ : nullptr;
}

const char* StoryEngineV2::lastError() const {
  return lastError_;
}

uint32_t StoryEngineV2::droppedEvents() const {
  return queue_.droppedCount();
}

bool StoryEngineV2::transitionTo(uint8_t nextStepIndex, uint32_t nowMs, const char* reason) {
  if (scenario_ == nullptr || nextStepIndex >= scenario_->stepCount) {
    return false;
  }
  previousStepIndex_ = currentStepIndex_;
  currentStepIndex_ = nextStepIndex;
  enteredAtMs_ = nowMs;
  stepChanged_ = true;
  if (reason != nullptr && reason[0] != '\0') {
    snprintf(lastTransitionId_, sizeof(lastTransitionId_), "%s", reason);
  } else {
    lastTransitionId_[0] = '\0';
  }
  Serial.printf("[STORY_V2] transition %s -> %s via=%s\n",
                scenario_->steps[previousStepIndex_].id,
                scenario_->steps[currentStepIndex_].id,
                reason != nullptr ? reason : "-");
  return true;
}

int8_t StoryEngineV2::selectEventTransition(const StoryEvent& event) const {
  if (scenario_ == nullptr || !running_) {
    return -1;
  }

  const StepDef& step = scenario_->steps[currentStepIndex_];
  int8_t selected = -1;
  uint8_t selectedPriority = 0U;
  for (uint8_t i = 0U; i < step.transitionCount; ++i) {
    const TransitionDef& transition = step.transitions[i];
    if (transition.trigger != StoryTransitionTrigger::kOnEvent) {
      continue;
    }
    if (transition.eventType != event.type) {
      continue;
    }
    if (!eventNameMatch(transition.eventName, event.name)) {
      continue;
    }
    if (selected < 0 || transition.priority > selectedPriority) {
      selected = static_cast<int8_t>(i);
      selectedPriority = transition.priority;
    }
  }
  return selected;
}

int8_t StoryEngineV2::selectImplicitTransition(uint32_t nowMs) const {
  if (scenario_ == nullptr || !running_) {
    return -1;
  }

  const StepDef& step = scenario_->steps[currentStepIndex_];
  int8_t selected = -1;
  uint8_t selectedPriority = 0U;
  for (uint8_t i = 0U; i < step.transitionCount; ++i) {
    const TransitionDef& transition = step.transitions[i];
    bool matched = false;
    if (transition.trigger == StoryTransitionTrigger::kImmediate) {
      matched = true;
    } else if (transition.trigger == StoryTransitionTrigger::kAfterMs) {
      matched = static_cast<uint32_t>(nowMs - enteredAtMs_) >= transition.afterMs;
    }
    if (!matched) {
      continue;
    }
    if (selected < 0 || transition.priority > selectedPriority) {
      selected = static_cast<int8_t>(i);
      selectedPriority = transition.priority;
    }
  }
  return selected;
}

uint32_t StoryEngineV2::computeNextDueAtMs(uint32_t nowMs) const {
  if (scenario_ == nullptr || !running_) {
    return 0U;
  }
  const StepDef& step = scenario_->steps[currentStepIndex_];
  uint32_t dueAtMs = 0U;
  for (uint8_t i = 0U; i < step.transitionCount; ++i) {
    const TransitionDef& transition = step.transitions[i];
    if (transition.trigger != StoryTransitionTrigger::kAfterMs) {
      continue;
    }
    const uint32_t candidate = nowMs + transition.afterMs;
    if (dueAtMs == 0U || candidate < dueAtMs) {
      dueAtMs = candidate;
    }
  }
  return dueAtMs;
}
