// scenario_manager.h - Story runtime wrapper for Freenove all-in-one.
#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

#include "core/scenario_def.h"

struct ScenarioSnapshot {
  const ScenarioDef* scenario = nullptr;
  const StepDef* step = nullptr;
  const char* screen_scene_id = nullptr;
  const char* audio_pack_id = nullptr;
  const char* const* action_ids = nullptr;
  uint8_t action_count = 0U;
  bool mp3_gate_open = false;
};

class ScenarioManager {
 public:
  static const char* readScenarioField(JsonVariantConst root,
                                       const char* const* candidates,
                                       size_t candidate_count);

  bool begin(const char* scenario_file_path);
  bool beginById(const char* scenario_id);
  void reset();
  void tick(uint32_t now_ms);

  void notifyUnlock(uint32_t now_ms);
  void notifyButton(uint8_t key, bool long_press, uint32_t now_ms);
  void notifyAudioDone(uint32_t now_ms);
  bool notifySerialEvent(const char* event_name, uint32_t now_ms);
  bool notifyTimerEvent(const char* event_name, uint32_t now_ms);
  bool notifyActionEvent(const char* event_name, uint32_t now_ms);

  ScenarioSnapshot snapshot() const;
  bool consumeSceneChanged();
  bool consumeAudioRequest(String* out_audio_pack_id);
  uint32_t transitionEventMask() const;

 private:
  struct StepResourceOverride {
    String step_id;
    String screen_scene_id;
    String audio_pack_id;
    static constexpr uint8_t kMaxActionOverrides = 8U;
    String action_ids[kMaxActionOverrides];
    const char* action_ptrs[kMaxActionOverrides] = {nullptr};
    uint8_t action_count = 0U;
  };

  static constexpr uint8_t kMaxStepResourceOverrides = 24U;

  void clearStepResourceOverrides();
  void loadStepResourceOverrides(const char* scenario_file_path);
  const StepResourceOverride* findStepResourceOverride(const char* step_id) const;
  void applyStepResourceOverride(const StepDef* step,
                                 const char** out_screen_scene_id,
                                 const char** out_audio_pack_id,
                                 const char* const** out_action_ids = nullptr,
                                 uint8_t* out_action_count = nullptr) const;

  bool dispatchEvent(StoryEventType type, const char* event_name, uint32_t now_ms, const char* source);
  bool applyTransition(const TransitionDef& transition, uint32_t now_ms, const char* source);
  bool runImmediateTransitions(uint32_t now_ms, const char* source);
  void evaluateAfterMsTransitions(uint32_t now_ms);
  void enterStep(int8_t step_index, uint32_t now_ms, const char* source);
  const StepDef* currentStep() const;
  bool transitionMatches(const TransitionDef& transition, StoryEventType type, const char* event_name) const;

  const ScenarioDef* scenario_ = nullptr;
  int8_t current_step_index_ = -1;
  uint32_t step_entered_at_ms_ = 0U;
  bool scene_changed_ = false;
  bool test_mode_ = false;
  bool timer_armed_ = false;
  bool timer_fired_ = false;
  uint32_t etape2_due_at_ms_ = 0U;
  String pending_audio_pack_;
  String initial_step_override_;
  StepResourceOverride step_resource_overrides_[kMaxStepResourceOverrides];
  uint8_t step_resource_override_count_ = 0U;
};
