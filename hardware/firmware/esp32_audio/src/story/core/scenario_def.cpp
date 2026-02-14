#include "scenario_def.h"

#include <cstring>

namespace {

bool sameText(const char* lhs, const char* rhs) {
  if (lhs == nullptr || rhs == nullptr) {
    return false;
  }
  return strcmp(lhs, rhs) == 0;
}

}  // namespace

int8_t storyFindStepIndex(const ScenarioDef& scenario, const char* stepId) {
  if (scenario.steps == nullptr || scenario.stepCount == 0U || stepId == nullptr || stepId[0] == '\0') {
    return -1;
  }
  for (uint8_t i = 0U; i < scenario.stepCount; ++i) {
    if (sameText(scenario.steps[i].id, stepId)) {
      return static_cast<int8_t>(i);
    }
  }
  return -1;
}

bool storyValidateScenarioDef(const ScenarioDef& scenario, StoryValidationError* outError) {
  StoryValidationError localError;
  localError.code = "OK";
  localError.detail = "";

  if (scenario.id == nullptr || scenario.id[0] == '\0') {
    localError.code = "SCENARIO_ID_EMPTY";
    localError.detail = "ScenarioDef.id is required";
    if (outError != nullptr) {
      *outError = localError;
    }
    return false;
  }

  if (scenario.steps == nullptr || scenario.stepCount == 0U) {
    localError.code = "SCENARIO_STEPS_EMPTY";
    localError.detail = "ScenarioDef.steps must not be empty";
    if (outError != nullptr) {
      *outError = localError;
    }
    return false;
  }

  if (storyFindStepIndex(scenario, scenario.initialStepId) < 0) {
    localError.code = "SCENARIO_INITIAL_STEP_INVALID";
    localError.detail = "ScenarioDef.initialStepId is missing or unknown";
    if (outError != nullptr) {
      *outError = localError;
    }
    return false;
  }

  for (uint8_t i = 0U; i < scenario.stepCount; ++i) {
    const StepDef& step = scenario.steps[i];
    if (step.id == nullptr || step.id[0] == '\0') {
      localError.code = "STEP_ID_EMPTY";
      localError.detail = "StepDef.id is required";
      if (outError != nullptr) {
        *outError = localError;
      }
      return false;
    }

    for (uint8_t j = static_cast<uint8_t>(i + 1U); j < scenario.stepCount; ++j) {
      if (sameText(step.id, scenario.steps[j].id)) {
        localError.code = "STEP_ID_DUPLICATE";
        localError.detail = step.id;
        if (outError != nullptr) {
          *outError = localError;
        }
        return false;
      }
    }

    for (uint8_t t = 0U; t < step.transitionCount; ++t) {
      const TransitionDef& tr = step.transitions[t];
      if (tr.targetStepId == nullptr || tr.targetStepId[0] == '\0') {
        localError.code = "TRANSITION_TARGET_EMPTY";
        localError.detail = step.id;
        if (outError != nullptr) {
          *outError = localError;
        }
        return false;
      }
      if (storyFindStepIndex(scenario, tr.targetStepId) < 0) {
        localError.code = "TRANSITION_TARGET_UNKNOWN";
        localError.detail = tr.targetStepId;
        if (outError != nullptr) {
          *outError = localError;
        }
        return false;
      }
      if (tr.trigger == StoryTransitionTrigger::kOnEvent &&
          tr.eventType == StoryEventType::kNone) {
        localError.code = "TRANSITION_EVENT_INVALID";
        localError.detail = tr.id;
        if (outError != nullptr) {
          *outError = localError;
        }
        return false;
      }
    }

    if (step.resources.appCount > 0U && step.resources.appIds == nullptr) {
      localError.code = "STEP_APPS_INVALID";
      localError.detail = step.id;
      if (outError != nullptr) {
        *outError = localError;
      }
      return false;
    }
  }

  if (outError != nullptr) {
    *outError = localError;
  }
  return true;
}
