#pragma once

#include <Arduino.h>

#include "../../audio/effects/audio_effect_id.h"
#include "../../services/audio/audio_service.h"
#include "../../story/apps/story_app_host.h"
#include "../../story/core/story_engine_v2.h"
#include "../../story/resources/action_registry.h"

class StoryControllerV2 {
 public:
  struct StoryControllerV2Snapshot {
    bool enabled = false;
    bool running = false;
    const char* scenarioId = nullptr;
    const char* stepId = nullptr;
    bool mp3GateOpen = true;
    uint8_t queueDepth = 0U;
    const char* appHostError = "OK";
    const char* engineError = "OK";
    uint32_t etape2DueMs = 0U;
    bool testMode = false;
  };

  struct Hooks {
    bool (*startRandomTokenBase)(const char* token,
                                 const char* source,
                                 bool allowSdFallback,
                                 uint32_t maxDurationMs) = nullptr;
    bool (*startFallbackBaseFx)(AudioEffectId effect,
                                uint32_t durationMs,
                                float gain,
                                const char* source) = nullptr;
    void (*applyAction)(const StoryActionDef& action, uint32_t nowMs, const char* source) = nullptr;
  };

  struct Options {
    const char* defaultScenarioId = "DEFAULT";
    const char* waitEtape2StepId = "STEP_WAIT_ETAPE2";
    const char* timerEventName = "ETAPE2_DUE";
    uint32_t etape2DelayMs = 15UL * 60UL * 1000UL;
    uint32_t etape2TestDelayMs = 5000U;
    float fallbackGain = 0.22f;
  };

  StoryControllerV2(AudioService& audio, const Hooks& hooks, const Options& options);

  bool begin(uint32_t nowMs);
  bool setScenario(const char* scenarioId, uint32_t nowMs, const char* source);
  void reset(uint32_t nowMs, const char* source);
  void onUnlock(uint32_t nowMs, const char* source);
  void update(uint32_t nowMs);

  bool isMp3GateOpen() const;
  void forceEtape2DueNow(uint32_t nowMs, const char* source);
  void setTestMode(bool enabled, uint32_t nowMs, const char* source);
  void setTestDelayMs(uint32_t delayMs, uint32_t nowMs, const char* source);

  bool jumpToStep(const char* stepId, uint32_t nowMs, const char* source);
  bool postSerialEvent(const char* eventName, uint32_t nowMs, const char* source);
  void printStatus(uint32_t nowMs, const char* source) const;
  void printScenarioList(const char* source) const;
  bool validateActiveScenario(const char* source) const;
  StoryControllerV2Snapshot snapshot(bool enabled, uint32_t nowMs) const;
  const char* healthLabel(bool enabled, uint32_t nowMs) const;
  void setTraceEnabled(bool enabled);
  bool traceEnabled() const;

  const char* scenarioId() const;
  const char* stepId() const;
  const char* activeScreenSceneId() const;
  const char* lastError() const;

 private:
  bool postEvent(StoryEventType type,
                 const char* eventName,
                 int32_t value,
                 uint32_t nowMs,
                 const char* source);
  bool postEventInternal(const StoryEvent& event,
                         const char* source,
                         bool notifyApps);
  void applyCurrentStep(uint32_t nowMs, const char* source);
  uint32_t activeDelayMs() const;
  void resetRuntimeState();
  StoryEventSink makeAppEventSink(const char* source);
  static bool postEventFromSink(const StoryEvent& event, void* user);
  bool isDuplicateStormEvent(const StoryEvent& event) const;

  AudioService& audio_;
  Hooks hooks_;
  Options options_;
  StoryEngineV2 engine_;
  StoryAppHost appHost_;
  bool initialized_ = false;
  bool testMode_ = false;
  uint32_t testDelayMs_ = 5000U;
  uint32_t etape2DueMs_ = 0U;
  bool etape2DuePosted_ = false;
  bool traceEnabled_ = false;
  StoryEvent lastPostedEvent_ = {};
  bool hasLastPostedEvent_ = false;
  uint32_t lastPostedEventAtMs_ = 0U;
  uint32_t droppedStormEvents_ = 0U;
  char activeScreenSceneId_[24] = {};
  char scenarioId_[20] = {};
};
