#include "story_app_host.h"

#include <cstring>

#include "../generated/apps_gen.h"
#include "../resources/action_registry.h"

namespace {

bool sameText(const char* lhs, const char* rhs) {
  if (lhs == nullptr || rhs == nullptr) {
    return false;
  }
  return strcmp(lhs, rhs) == 0;
}

void safeCopy(char* out, size_t outLen, const char* in) {
  if (out == nullptr || outLen == 0U) {
    return;
  }
  out[0] = '\0';
  if (in == nullptr || in[0] == '\0') {
    return;
  }
  snprintf(out, outLen, "%s", in);
}

}  // namespace

bool StoryAppHost::begin(const StoryAppContext& context) {
  context_ = context;
  initialized_ = laDetectorApp_.begin(context_) && audioPackApp_.begin(context_) &&
                 screenSceneApp_.begin(context_) && mp3GateApp_.begin(context_);
  activeCount_ = 0U;
  safeCopy(lastError_, sizeof(lastError_), initialized_ ? "OK" : "APP_BEGIN_FAIL");
  lastDetail_[0] = '\0';
  return initialized_;
}

void StoryAppHost::stopAll(const char* reason) {
  if (activeCount_ == 0U) {
    return;
  }
  for (uint8_t i = 0U; i < activeCount_; ++i) {
    if (activeApps_[i] != nullptr) {
      activeApps_[i]->stop(reason);
      activeApps_[i] = nullptr;
    }
  }
  activeCount_ = 0U;
}

bool StoryAppHost::startStep(const ScenarioDef* scenario,
                             const StepDef* step,
                             uint32_t nowMs,
                             const char* source) {
  if (!initialized_ || scenario == nullptr || step == nullptr) {
    setError("HOST_NOT_READY", "startStep");
    return false;
  }

  stopAll("step_change");

  for (uint8_t i = 0U; i < step->resources.actionCount; ++i) {
    const char* actionId = (step->resources.actionIds != nullptr) ? step->resources.actionIds[i] : nullptr;
    const StoryActionDef* action = storyFindAction(actionId);
    if (action == nullptr) {
      continue;
    }
    if (context_.applyAction != nullptr) {
      context_.applyAction(*action, nowMs, source);
    }
  }

  if (step->resources.appCount == 0U || step->resources.appIds == nullptr) {
    safeCopy(lastError_, sizeof(lastError_), "OK");
    lastDetail_[0] = '\0';
    return true;
  }

  for (uint8_t i = 0U; i < step->resources.appCount; ++i) {
    const char* appBindingId = step->resources.appIds[i];
    const AppBindingDef* binding = generatedAppBindingById(appBindingId);
    if (binding == nullptr) {
      setError("APP_BINDING_UNKNOWN", appBindingId);
      return false;
    }
    if (!startBinding(*binding, scenario, step, nowMs, source)) {
      if (lastError_[0] == '\0' || sameText(lastError_, "OK")) {
        setError("APP_START_FAILED", appBindingId);
      }
      return false;
    }
  }

  safeCopy(lastError_, sizeof(lastError_), "OK");
  lastDetail_[0] = '\0';
  return true;
}

void StoryAppHost::update(uint32_t nowMs, const StoryEventSink& sink) {
  for (uint8_t i = 0U; i < activeCount_; ++i) {
    StoryApp* app = activeApps_[i];
    if (app != nullptr) {
      app->update(nowMs, sink);
    }
  }
}

void StoryAppHost::handleEvent(const StoryEvent& event, const StoryEventSink& sink) {
  for (uint8_t i = 0U; i < activeCount_; ++i) {
    StoryApp* app = activeApps_[i];
    if (app != nullptr) {
      app->handleEvent(event, sink);
    }
  }
}

const char* StoryAppHost::activeScreenSceneId() const {
  return screenSceneApp_.activeSceneId();
}

bool StoryAppHost::validateScenario(const ScenarioDef& scenario, StoryAppValidation* outValidation) const {
  StoryAppValidation local = {};
  local.ok = true;
  local.code = "OK";
  local.detail = "";

  for (uint8_t i = 0U; i < scenario.stepCount; ++i) {
    const StepDef& step = scenario.steps[i];
    if (step.resources.appCount > 0U && step.resources.appIds == nullptr) {
      local.ok = false;
      local.code = "STEP_APPS_PTR_NULL";
      local.detail = step.id;
      break;
    }
    for (uint8_t a = 0U; a < step.resources.appCount; ++a) {
      const char* bindingId = step.resources.appIds[a];
      const AppBindingDef* binding = generatedAppBindingById(bindingId);
      if (binding == nullptr) {
        local.ok = false;
        local.code = "APP_BINDING_UNKNOWN";
        local.detail = bindingId;
        break;
      }
      const bool supportedType =
          (binding->type == StoryAppType::kLaDetector || binding->type == StoryAppType::kAudioPack ||
           binding->type == StoryAppType::kScreenScene || binding->type == StoryAppType::kMp3Gate);
      if (!supportedType) {
        local.ok = false;
        local.code = "APP_BINDING_UNSUPPORTED";
        local.detail = bindingId;
        break;
      }
      if (binding->type == StoryAppType::kLaDetector) {
        const LaDetectorAppConfigDef* cfg = generatedLaDetectorConfigByBindingId(bindingId);
        if (cfg == nullptr) {
          local.ok = false;
          local.code = "APP_LA_CONFIG_MISSING";
          local.detail = bindingId;
          break;
        }
        if (cfg->holdMs < 100U || cfg->holdMs > 60000U) {
          local.ok = false;
          local.code = "APP_LA_HOLD_INVALID";
          local.detail = bindingId;
          break;
        }
        if (cfg->unlockEvent == nullptr || cfg->unlockEvent[0] == '\0') {
          local.ok = false;
          local.code = "APP_LA_EVENT_INVALID";
          local.detail = bindingId;
          break;
        }
      }
    }
    if (!local.ok) {
      break;
    }
  }

  if (outValidation != nullptr) {
    *outValidation = local;
  }
  return local.ok;
}

const char* StoryAppHost::lastError() const {
  return (lastError_[0] != '\0') ? lastError_ : "OK";
}

StoryApp* StoryAppHost::appForType(StoryAppType type) {
  switch (type) {
    case StoryAppType::kLaDetector:
      return &laDetectorApp_;
    case StoryAppType::kAudioPack:
      return &audioPackApp_;
    case StoryAppType::kScreenScene:
      return &screenSceneApp_;
    case StoryAppType::kMp3Gate:
      return &mp3GateApp_;
    case StoryAppType::kNone:
    default:
      return nullptr;
  }
}

bool StoryAppHost::startBinding(const AppBindingDef& binding,
                                const ScenarioDef* scenario,
                                const StepDef* step,
                                uint32_t nowMs,
                                const char* source) {
  StoryApp* app = appForType(binding.type);
  if (app == nullptr) {
    setError("APP_TYPE_UNSUPPORTED", binding.id);
    return false;
  }

  StoryStepContext stepContext = {};
  stepContext.scenario = scenario;
  stepContext.step = step;
  stepContext.binding = &binding;
  stepContext.nowMs = nowMs;
  stepContext.source = source;
  app->start(stepContext);

  for (uint8_t i = 0U; i < activeCount_; ++i) {
    if (activeApps_[i] == app) {
      return true;
    }
  }

  if (activeCount_ >= kMaxActiveApps) {
    setError("APP_HOST_OVERFLOW", binding.id);
    return false;
  }

  activeApps_[activeCount_++] = app;
  return true;
}

void StoryAppHost::setError(const char* code, const char* detail) {
  safeCopy(lastError_, sizeof(lastError_), code);
  safeCopy(lastDetail_, sizeof(lastDetail_), detail);
}
