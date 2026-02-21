#pragma once

#include <Arduino.h>

#include "audio_pack_app.h"
#include "espnow_stack_app.h"
#include "la_detector_app.h"
#include "mp3_gate_app.h"
#include "screen_scene_app.h"
#include "story_app.h"
#include "wifi_stack_app.h"

struct StoryAppValidation {
  bool ok = true;
  const char* code = "OK";
  const char* detail = "";
};

class StoryAppHost {
 public:
  bool begin(const StoryAppContext& context);
  void stopAll(const char* reason);
  bool startStep(const ScenarioDef* scenario, const StepDef* step, uint32_t nowMs, const char* source);
  void update(uint32_t nowMs, const StoryEventSink& sink);
  void handleEvent(const StoryEvent& event, const StoryEventSink& sink);

  const char* activeScreenSceneId() const;
  bool validateScenario(const ScenarioDef& scenario, StoryAppValidation* outValidation) const;
  const char* lastError() const;

 private:
  static constexpr uint8_t kMaxActiveApps = 6U;

  StoryApp* appForType(StoryAppType type);
  bool startBinding(const AppBindingDef& binding,
                    const ScenarioDef* scenario,
                    const StepDef* step,
                    uint32_t nowMs,
                    const char* source);
  void setError(const char* code, const char* detail);

  StoryAppContext context_ = {};
  bool initialized_ = false;
  StoryApp* activeApps_[kMaxActiveApps] = {};
  uint8_t activeCount_ = 0U;
  char lastError_[32] = "OK";
  char lastDetail_[40] = "";

  LaDetectorApp laDetectorApp_;
  AudioPackApp audioPackApp_;
  ScreenSceneApp screenSceneApp_;
  Mp3GateApp mp3GateApp_;
  WifiStackApp wifiStackApp_;
  EspNowStackApp espNowStackApp_;
};
