#pragma once

#include <Arduino.h>

#include "../../audio/effects/audio_effect_id.h"
#include "../../services/audio/audio_service.h"
#include "../../services/la/la_detector_runtime_service.h"
#include "../core/scenario_def.h"
#include "../resources/action_registry.h"

struct StoryAppContext {
  AudioService* audioService = nullptr;
  bool (*startRandomTokenBase)(const char* token,
                               const char* source,
                               bool allowSdFallback,
                               uint32_t maxDurationMs) = nullptr;
  bool (*startFallbackBaseFx)(AudioEffectId effect,
                              uint32_t durationMs,
                              float gain,
                              const char* source) = nullptr;
  void (*applyAction)(const StoryActionDef& action, uint32_t nowMs, const char* source) = nullptr;
  LaDetectorRuntimeService* laRuntime = nullptr;
  void (*onUnlockRuntimeApplied)(uint32_t nowMs, const char* source) = nullptr;
};

struct StoryStepContext {
  const ScenarioDef* scenario = nullptr;
  const StepDef* step = nullptr;
  const AppBindingDef* binding = nullptr;
  uint32_t nowMs = 0U;
  const char* source = nullptr;
};

struct StoryAppSnapshot {
  const char* bindingId = nullptr;
  bool active = false;
  const char* status = "IDLE";
  uint32_t startedAtMs = 0U;
};

struct StoryEventSink {
  bool (*postFn)(const StoryEvent& event, void* user) = nullptr;
  void* user = nullptr;

  bool post(const StoryEvent& event) const {
    if (postFn == nullptr) {
      return false;
    }
    return postFn(event, user);
  }

  bool emit(StoryEventType type, const char* name, int32_t value, uint32_t atMs) const {
    StoryEvent event = {};
    event.type = type;
    event.value = value;
    event.atMs = atMs;
    if (name != nullptr && name[0] != '\0') {
      snprintf(event.name, sizeof(event.name), "%s", name);
    }
    return post(event);
  }
};

class StoryApp {
 public:
  virtual ~StoryApp() = default;

  virtual bool begin(const StoryAppContext& context) = 0;
  virtual void start(const StoryStepContext& stepContext) = 0;
  virtual void update(uint32_t nowMs, const StoryEventSink& sink) = 0;
  virtual void stop(const char* reason) = 0;
  virtual bool handleEvent(const StoryEvent& event, const StoryEventSink& sink) = 0;
  virtual StoryAppSnapshot snapshot() const = 0;
};
