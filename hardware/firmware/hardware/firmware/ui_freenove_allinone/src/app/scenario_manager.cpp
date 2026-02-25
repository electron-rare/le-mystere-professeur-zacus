// scenario_manager.cpp - Story transitions + timing hooks.
#include "scenario_manager.h"

#include <ArduinoJson.h>
#include <LittleFS.h>
#include <cstdio>
#include <cstring>

#include "resources/screen_scene_registry.h"
#include "scenarios/default_scenario_v2.h"

namespace {

constexpr uint32_t kEtape2DelayMs = 15UL * 60UL * 1000UL;
constexpr uint32_t kEtape2TestDelayMs = 5000U;
constexpr uint32_t kWinDueDelayMs = 10UL * 60UL * 1000UL;

bool eventNameMatches(const char* expected, const char* actual) {
  if (expected == nullptr || expected[0] == '\0') {
    return true;
  }
  if (actual == nullptr) {
    return false;
  }
  return std::strcmp(expected, actual) == 0;
}

const char* stringOrNull(JsonVariantConst value) {
  if (!value.is<const char*>()) {
    return nullptr;
  }
  const char* text = value.as<const char*>();
  if (text == nullptr || text[0] == '\0') {
    return nullptr;
  }
  return text;
}

bool loadScenarioIdFromFile(const char* scenario_file_path, String* out_scenario_id) {
  if (scenario_file_path == nullptr || scenario_file_path[0] == '\0' || out_scenario_id == nullptr) {
    return false;
  }
  if (!LittleFS.exists(scenario_file_path)) {
    return false;
  }

  File file = LittleFS.open(scenario_file_path, "r");
  if (!file) {
    Serial.printf("[SCENARIO] failed to open scenario config: %s\n", scenario_file_path);
    return false;
  }
  const size_t file_size = static_cast<size_t>(file.size());
  if (file_size == 0U || file_size > 12288U) {
    file.close();
    Serial.printf("[SCENARIO] unexpected scenario config size: %s (%u bytes)\n",
                  scenario_file_path,
                  static_cast<unsigned int>(file_size));
    return false;
  }

  DynamicJsonDocument document(file_size + 512U);
  const DeserializationError error = deserializeJson(document, file);
  file.close();
  if (error) {
    Serial.printf("[SCENARIO] invalid scenario config json (%s): %s\n",
                  scenario_file_path,
                  error.c_str());
    return false;
  }

  const char* const id_candidates[] = {"scenario", "scenario_id", "id"};
  const char* scenario_id = ScenarioManager::readScenarioField(
      document.as<JsonVariantConst>(), id_candidates, sizeof(id_candidates) / sizeof(id_candidates[0]));
  if (scenario_id == nullptr || scenario_id[0] == '\0') {
    Serial.printf("[SCENARIO] missing scenario id in config: %s\n", scenario_file_path);
    return false;
  }

  *out_scenario_id = scenario_id;
  return true;
}

}  // namespace

const char* ScenarioManager::readScenarioField(JsonVariantConst root,
                                               const char* const* candidates,
                                               size_t candidate_count) {
  if (candidates == nullptr || candidate_count == 0U || root.isNull()) {
    return nullptr;
  }
  JsonObjectConst object = root.as<JsonObjectConst>();
  if (object.isNull()) {
    return nullptr;
  }
  for (size_t index = 0U; index < candidate_count; ++index) {
    const char* key = candidates[index];
    if (key == nullptr || key[0] == '\0') {
      continue;
    }
    JsonVariantConst candidate = object[key];
    if (!candidate.is<const char*>()) {
      continue;
    }
    const char* text = candidate.as<const char*>();
    if (text != nullptr && text[0] != '\0') {
      return text;
    }
  }
  return nullptr;
}

bool ScenarioManager::begin(const char* scenario_file_path) {
  scenario_ = nullptr;
  initial_step_override_.remove(0);
  clearStepResourceOverrides();
  String selected_scenario_id;
  if (loadScenarioIdFromFile(scenario_file_path, &selected_scenario_id)) {
    scenario_ = storyScenarioV2ById(selected_scenario_id.c_str());
    if (scenario_ != nullptr) {
      Serial.printf("[SCENARIO] selected id from %s: %s\n",
                    scenario_file_path,
                    selected_scenario_id.c_str());
    } else {
      Serial.printf("[SCENARIO] unknown id in %s: %s (fallback default)\n",
                    scenario_file_path,
                    selected_scenario_id.c_str());
    }
  } else if (scenario_file_path != nullptr && scenario_file_path[0] != '\0') {
    Serial.printf("[SCENARIO] no valid scenario config at %s (fallback default)\n", scenario_file_path);
  }

  if (scenario_ == nullptr) {
    scenario_ = storyScenarioV2Default();
  }
  if (scenario_ == nullptr) {
    Serial.println("[SCENARIO] default scenario unavailable");
    return false;
  }

  if (storyValidateScenarioDef(*scenario_, nullptr)) {
    Serial.printf("[SCENARIO] loaded built-in scenario: %s v%u (%u steps)\n",
                  scenario_->id,
                  scenario_->version,
                  scenario_->stepCount);
  } else {
    Serial.printf("[SCENARIO] warning: validation failed for %s\n", scenario_->id);
  }

  loadStepResourceOverrides(scenario_file_path);
  reset();
  return true;
}

bool ScenarioManager::beginById(const char* scenario_id) {
  scenario_ = nullptr;
  initial_step_override_.remove(0);
  clearStepResourceOverrides();

  if (scenario_id != nullptr && scenario_id[0] != '\0') {
    scenario_ = storyScenarioV2ById(scenario_id);
  }
  if (scenario_ == nullptr) {
    Serial.printf("[SCENARIO] unknown scenario id: %s\n", (scenario_id != nullptr) ? scenario_id : "null");
    return false;
  }

  if (storyValidateScenarioDef(*scenario_, nullptr)) {
    Serial.printf("[SCENARIO] loaded built-in scenario by id: %s v%u (%u steps)\n",
                  scenario_->id,
                  scenario_->version,
                  scenario_->stepCount);
  } else {
    Serial.printf("[SCENARIO] warning: validation failed for %s\n", scenario_->id);
  }
  reset();
  return true;
}

void ScenarioManager::reset() {
  if (scenario_ == nullptr) {
    return;
  }
  const char* initial_step_id = scenario_->initialStepId;
  if (!initial_step_override_.isEmpty()) {
    initial_step_id = initial_step_override_.c_str();
  }
  current_step_index_ = storyFindStepIndex(*scenario_, initial_step_id);
  if (current_step_index_ < 0 && scenario_->stepCount > 0U) {
    current_step_index_ = 0;
  }
  step_entered_at_ms_ = millis();
  pending_audio_pack_.remove(0);
  forced_screen_scene_id_.remove(0);
  scene_changed_ = true;
  timer_armed_ = false;
  timer_fired_ = false;
  etape2_due_at_ms_ = 0U;
  win_due_armed_ = false;
  win_due_fired_ = false;
  win_due_at_ms_ = 0U;

  const ScenarioSnapshot state = snapshot();
  if (state.audio_pack_id != nullptr && state.audio_pack_id[0] != '\0') {
    pending_audio_pack_ = state.audio_pack_id;
  }
}

void ScenarioManager::tick(uint32_t now_ms) {
  if (scenario_ == nullptr || current_step_index_ < 0) {
    return;
  }
  evaluateAfterMsTransitions(now_ms);
  if (timer_armed_ && !timer_fired_ && etape2_due_at_ms_ > 0U && now_ms >= etape2_due_at_ms_) {
    timer_fired_ = true;
    dispatchEvent(StoryEventType::kTimer, "ETAPE2_DUE", now_ms, "timer_due");
  }
  if (win_due_armed_ && !win_due_fired_ && win_due_at_ms_ > 0U && now_ms >= win_due_at_ms_) {
    win_due_fired_ = true;
    dispatchEvent(StoryEventType::kTimer, "WIN_DUE", now_ms, "timer_win_due");
  }
}

void ScenarioManager::notifyUnlock(uint32_t now_ms) {
  (void)notifyUnlockEvent("UNLOCK", now_ms);
}

bool ScenarioManager::notifyUnlockEvent(const char* event_name, uint32_t now_ms) {
  timer_armed_ = true;
  timer_fired_ = false;
  etape2_due_at_ms_ = now_ms + (test_mode_ ? kEtape2TestDelayMs : kEtape2DelayMs);
  const char* name = (event_name != nullptr && event_name[0] != '\0') ? event_name : "UNLOCK";
  return dispatchEvent(StoryEventType::kUnlock, name, now_ms, "unlock_event");
}

void ScenarioManager::notifyButton(uint8_t key, bool long_press, uint32_t now_ms) {
  const StepDef* step = currentStep();

  if (dispatchEvent(StoryEventType::kButton, "ANY", now_ms, long_press ? "btn_any_long" : "btn_any_short")) {
    return;
  }
  char button_event[24] = {0};
  std::snprintf(button_event,
                sizeof(button_event),
                "BTN%u_%s",
                static_cast<unsigned int>(key),
                long_press ? "LONG" : "SHORT");
  if (dispatchEvent(StoryEventType::kButton, button_event, now_ms, "btn_specific")) {
    return;
  }

  if (step != nullptr && key >= 1U && key <= 5U && step->id != nullptr) {
    const char* screen_scene_id = step->resources.screenSceneId;
    if (std::strcmp(step->id, "STEP_WAIT_ETAPE2") == 0) {
      timer_armed_ = true;
      timer_fired_ = false;
      etape2_due_at_ms_ = now_ms + (test_mode_ ? kEtape2TestDelayMs : kEtape2DelayMs);
      return;
    }
    if (screen_scene_id != nullptr &&
        (std::strcmp(screen_scene_id, "SCENE_LA_DETECTOR") == 0 ||
         std::strcmp(screen_scene_id, "SCENE_LA_DETECT") == 0)) {
      timer_armed_ = true;
      timer_fired_ = false;
      etape2_due_at_ms_ = now_ms + (test_mode_ ? kEtape2TestDelayMs : kEtape2DelayMs);
      return;
    }
    if (std::strcmp(step->id, "STEP_WAIT_UNLOCK") == 0) {
      // Contract: any short or long press from lock screen jumps to LA detector.
      if (dispatchEvent(StoryEventType::kSerial, "BTN_NEXT", now_ms, "btn_any_short")) {
        return;
      }
      dispatchEvent(StoryEventType::kSerial, "NEXT", now_ms, "btn_any_short_legacy");
      return;
    }
  }
}

void ScenarioManager::notifyAudioDone(uint32_t now_ms) {
  dispatchEvent(StoryEventType::kAudioDone, "AUDIO_DONE", now_ms, "audio_done");
}

bool ScenarioManager::notifyButtonEvent(const char* event_name, uint32_t now_ms) {
  const char* name = (event_name != nullptr && event_name[0] != '\0') ? event_name : "ANY";
  return dispatchEvent(StoryEventType::kButton, name, now_ms, "button_event");
}

bool ScenarioManager::notifyEspNowEvent(const char* event_name, uint32_t now_ms) {
  const char* name = (event_name != nullptr && event_name[0] != '\0') ? event_name : "EVENT";
  return dispatchEvent(StoryEventType::kEspNow, name, now_ms, "espnow_event");
}

bool ScenarioManager::notifySerialEvent(const char* event_name, uint32_t now_ms) {
  const char* name = (event_name != nullptr && event_name[0] != '\0') ? event_name : "SERIAL_EVENT";
  return dispatchEvent(StoryEventType::kSerial, name, now_ms, "serial_event");
}

bool ScenarioManager::notifyTimerEvent(const char* event_name, uint32_t now_ms) {
  const char* name = (event_name != nullptr && event_name[0] != '\0') ? event_name : "TIMER_EVENT";
  return dispatchEvent(StoryEventType::kTimer, name, now_ms, "timer_event");
}

bool ScenarioManager::notifyActionEvent(const char* event_name, uint32_t now_ms) {
  const char* name = (event_name != nullptr && event_name[0] != '\0') ? event_name : "ACTION_EVENT";
  return dispatchEvent(StoryEventType::kAction, name, now_ms, "action_event");
}

bool ScenarioManager::gotoScene(const char* scene_id, uint32_t now_ms, const char* source) {
  if (scenario_ == nullptr || scene_id == nullptr || scene_id[0] == '\0') {
    return false;
  }
  for (uint8_t index = 0U; index < scenario_->stepCount; ++index) {
    const StepDef& step = scenario_->steps[index];
    const char* screen_scene_id = step.resources.screenSceneId;
    const char* audio_pack_id = step.resources.audioPackId;
    applyStepResourceOverride(&step, &screen_scene_id, &audio_pack_id);
    (void)audio_pack_id;
    if (screen_scene_id == nullptr || std::strcmp(screen_scene_id, scene_id) != 0) {
      continue;
    }
    const char* enter_source =
        (source != nullptr && source[0] != '\0') ? source : "scene_goto";
    enterStep(static_cast<int8_t>(index), now_ms, enter_source, "scene_goto");
    runImmediateTransitions(now_ms, enter_source, "scene_goto");
    return true;
  }
  const char* normalized_scene = storyNormalizeScreenSceneId(scene_id);
  if (normalized_scene == nullptr) {
    return false;
  }
  if (currentStep() == nullptr) {
    return false;
  }
  forced_screen_scene_id_ = normalized_scene;
  scene_changed_ = true;
  Serial.printf("[SCENARIO] scene override via=%s id=%s\n",
                (source != nullptr && source[0] != '\0') ? source : "scene_goto",
                normalized_scene);
  return true;
}

ScenarioSnapshot ScenarioManager::snapshot() const {
  ScenarioSnapshot out;
  out.scenario = scenario_;
  out.step = currentStep();
  if (out.step != nullptr) {
    const char* screen_scene_id = out.step->resources.screenSceneId;
    const char* audio_pack_id = out.step->resources.audioPackId;
    const char* const* action_ids = out.step->resources.actionIds;
    uint8_t action_count = out.step->resources.actionCount;
    applyStepResourceOverride(out.step, &screen_scene_id, &audio_pack_id, &action_ids, &action_count);
    out.screen_scene_id = screen_scene_id;
    if (!forced_screen_scene_id_.isEmpty()) {
      out.screen_scene_id = forced_screen_scene_id_.c_str();
    }
    out.audio_pack_id = audio_pack_id;
    out.action_ids = action_ids;
    out.action_count = action_count;
    out.mp3_gate_open = out.step->mp3GateOpen;
  }
  return out;
}

bool ScenarioManager::consumeSceneChanged() {
  const bool changed = scene_changed_;
  scene_changed_ = false;
  return changed;
}

bool ScenarioManager::consumeAudioRequest(String* out_audio_pack_id) {
  if (pending_audio_pack_.isEmpty()) {
    return false;
  }
  if (out_audio_pack_id != nullptr) {
    *out_audio_pack_id = pending_audio_pack_;
  }
  pending_audio_pack_.remove(0);
  return true;
}

uint32_t ScenarioManager::transitionEventMask() const {
  if (scenario_ == nullptr || scenario_->steps == nullptr) {
    return 0U;
  }
  uint32_t mask = 0U;
  for (uint8_t step_index = 0; step_index < scenario_->stepCount; ++step_index) {
    const StepDef& step = scenario_->steps[step_index];
    if (step.transitions == nullptr || step.transitionCount == 0U) {
      continue;
    }
    for (uint8_t transition_index = 0; transition_index < step.transitionCount; ++transition_index) {
      const TransitionDef& transition = step.transitions[transition_index];
      if (transition.trigger != StoryTransitionTrigger::kOnEvent &&
          transition.trigger != StoryTransitionTrigger::kAfterMs) {
        continue;
      }
      const uint8_t event_index = static_cast<uint8_t>(transition.eventType);
      if (event_index >= 31U) {
        continue;
      }
      mask |= (1UL << event_index);
    }
  }
  return mask;
}

bool ScenarioManager::dispatchEvent(StoryEventType type,
                                    const char* event_name,
                                    uint32_t now_ms,
                                    const char* source) {
  const StepDef* step = currentStep();
  if (step == nullptr || step->transitionCount == 0U) {
    return false;
  }

  const TransitionDef* selected = nullptr;
  for (uint8_t i = 0; i < step->transitionCount; ++i) {
    const TransitionDef& transition = step->transitions[i];
    if (!transitionMatches(transition, type, event_name)) {
      continue;
    }
    if (selected == nullptr || transition.priority > selected->priority) {
      selected = &transition;
    }
  }
  if (selected == nullptr) {
    return false;
  }
  if (!applyTransition(*selected, now_ms, source, event_name)) {
    return false;
  }
  runImmediateTransitions(now_ms, source, event_name);
  return true;
}

bool ScenarioManager::applyTransition(const TransitionDef& transition,
                                      uint32_t now_ms,
                                      const char* source,
                                      const char* event_name) {
  if (scenario_ == nullptr || transition.targetStepId == nullptr) {
    return false;
  }
  const int8_t target = storyFindStepIndex(*scenario_, transition.targetStepId);
  if (target < 0) {
    Serial.printf("[SCENARIO] invalid transition target: %s\n", transition.targetStepId);
    return false;
  }
  enterStep(target, now_ms, source, event_name);
  return true;
}

bool ScenarioManager::runImmediateTransitions(uint32_t now_ms, const char* source, const char* parent_event_name) {
  bool moved = false;
  uint8_t hop_count = 0U;
  for (uint8_t guard = 0; guard < 8U; ++guard) {
    const StepDef* step = currentStep();
    if (step == nullptr || step->transitionCount == 0U) {
      break;
    }
    const TransitionDef* selected = nullptr;
    for (uint8_t i = 0; i < step->transitionCount; ++i) {
      const TransitionDef& transition = step->transitions[i];
      if (transition.trigger != StoryTransitionTrigger::kImmediate) {
        continue;
      }
      if (selected == nullptr || transition.priority > selected->priority) {
        selected = &transition;
      }
    }
    if (selected == nullptr) {
      break;
    }
    ++hop_count;
    if (!applyTransition(*selected, now_ms, source, parent_event_name != nullptr ? parent_event_name : "immediate")) {
      break;
    }
    moved = true;
  }
  if (hop_count > 1U) {
    Serial.printf("[SCENARIO] immediate_chain hops=%u source=%s event=%s\n",
                  static_cast<unsigned int>(hop_count),
                  (source != nullptr && source[0] != '\0') ? source : "-",
                  (parent_event_name != nullptr && parent_event_name[0] != '\0') ? parent_event_name : "-");
  }
  return moved;
}

void ScenarioManager::evaluateAfterMsTransitions(uint32_t now_ms) {
  const StepDef* step = currentStep();
  if (step == nullptr || step->transitionCount == 0U) {
    return;
  }

  const TransitionDef* selected = nullptr;
  for (uint8_t i = 0; i < step->transitionCount; ++i) {
    const TransitionDef& transition = step->transitions[i];
    if (transition.trigger != StoryTransitionTrigger::kAfterMs) {
      continue;
    }
    if (now_ms - step_entered_at_ms_ < transition.afterMs) {
      continue;
    }
    if (selected == nullptr || transition.priority > selected->priority) {
      selected = &transition;
    }
  }
  if (selected != nullptr) {
    if (applyTransition(*selected, now_ms, "after_ms", "after_ms")) {
      runImmediateTransitions(now_ms, "after_ms", "after_ms");
    }
  }
}

void ScenarioManager::enterStep(int8_t step_index, uint32_t now_ms, const char* source, const char* event_name) {
  if (scenario_ == nullptr || step_index < 0 || step_index >= static_cast<int8_t>(scenario_->stepCount)) {
    return;
  }

  const StepDef* previous_step = currentStep();
  const char* from_step = (previous_step != nullptr && previous_step->id != nullptr) ? previous_step->id : "n/a";
  const char* from_scene = (previous_step != nullptr) ? previous_step->resources.screenSceneId : nullptr;
  const char* from_pack = (previous_step != nullptr) ? previous_step->resources.audioPackId : nullptr;
  if (previous_step != nullptr) {
    applyStepResourceOverride(previous_step, &from_scene, &from_pack);
  }
  if (!forced_screen_scene_id_.isEmpty()) {
    from_scene = forced_screen_scene_id_.c_str();
  }

  current_step_index_ = step_index;
  step_entered_at_ms_ = now_ms;
  scene_changed_ = true;

  const StepDef* step = currentStep();
  if (step == nullptr) {
    return;
  }

  pending_audio_pack_.remove(0);
  forced_screen_scene_id_.remove(0);
  const char* screen_scene_id = step->resources.screenSceneId;
  const char* audio_pack_id = step->resources.audioPackId;
  applyStepResourceOverride(step, &screen_scene_id, &audio_pack_id);
  if (audio_pack_id != nullptr && audio_pack_id[0] != '\0') {
    pending_audio_pack_ = audio_pack_id;
  }
  win_due_armed_ = false;
  win_due_fired_ = false;
  win_due_at_ms_ = 0U;
  if (screen_scene_id != nullptr && std::strcmp(screen_scene_id, "SCENE_FINAL_WIN") == 0) {
    win_due_armed_ = true;
    win_due_fired_ = false;
    win_due_at_ms_ = now_ms + kWinDueDelayMs;
  }
  Serial.printf("[SCENARIO] transition from_step=%s to_step=%s from_scene=%s to_scene=%s event=%s source=%s audio_pack=%s\n",
                from_step,
                step->id != nullptr ? step->id : "n/a",
                (from_scene != nullptr && from_scene[0] != '\0') ? from_scene : "n/a",
                (screen_scene_id != nullptr && screen_scene_id[0] != '\0') ? screen_scene_id : "n/a",
                (event_name != nullptr && event_name[0] != '\0') ? event_name : "-",
                (source != nullptr && source[0] != '\0') ? source : "-",
                (audio_pack_id != nullptr && audio_pack_id[0] != '\0') ? audio_pack_id : "n/a");
}

const StepDef* ScenarioManager::currentStep() const {
  if (scenario_ == nullptr || current_step_index_ < 0 || current_step_index_ >= static_cast<int8_t>(scenario_->stepCount)) {
    return nullptr;
  }
  return &scenario_->steps[current_step_index_];
}

bool ScenarioManager::transitionMatches(const TransitionDef& transition,
                                        StoryEventType type,
                                        const char* event_name) const {
  if (transition.trigger != StoryTransitionTrigger::kOnEvent) {
    return false;
  }
  if (transition.eventType != type) {
    return false;
  }
  return eventNameMatches(transition.eventName, event_name);
}

void ScenarioManager::clearStepResourceOverrides() {
  for (uint8_t index = 0; index < step_resource_override_count_; ++index) {
    step_resource_overrides_[index].step_id.remove(0);
    step_resource_overrides_[index].screen_scene_id.remove(0);
    step_resource_overrides_[index].audio_pack_id.remove(0);
    step_resource_overrides_[index].action_count = 0U;
    for (uint8_t action_index = 0; action_index < StepResourceOverride::kMaxActionOverrides; ++action_index) {
      step_resource_overrides_[index].action_ids[action_index].remove(0);
      step_resource_overrides_[index].action_ptrs[action_index] = nullptr;
    }
  }
  step_resource_override_count_ = 0U;
}

void ScenarioManager::loadStepResourceOverrides(const char* scenario_file_path) {
  clearStepResourceOverrides();
  if (scenario_file_path == nullptr || scenario_file_path[0] == '\0') {
    return;
  }
  if (!LittleFS.exists(scenario_file_path)) {
    return;
  }

  File file = LittleFS.open(scenario_file_path, "r");
  if (!file) {
    return;
  }
  const size_t file_size = static_cast<size_t>(file.size());
  if (file_size == 0U || file_size > 12288U) {
    file.close();
    return;
  }

  DynamicJsonDocument document(file_size + 1024U);
  const DeserializationError error = deserializeJson(document, file);
  file.close();
  if (error) {
    Serial.printf("[SCENARIO] override parse failed (%s): %s\n", scenario_file_path, error.c_str());
    return;
  }

  const char* const initial_step_keys[] = {"initial_step", "initialStepId"};
  const char* initial_step =
      readScenarioField(document.as<JsonVariantConst>(), initial_step_keys, sizeof(initial_step_keys) / sizeof(initial_step_keys[0]));
  if (initial_step != nullptr) {
    initial_step_override_ = initial_step;
    Serial.printf("[SCENARIO] override initial_step=%s\n", initial_step_override_.c_str());
  }

  JsonArrayConst steps = document["steps"].as<JsonArrayConst>();
  if (steps.isNull()) {
    return;
  }

  for (JsonVariantConst variant : steps) {
    if (!variant.is<JsonObjectConst>()) {
      continue;
    }
    JsonObjectConst step_obj = variant.as<JsonObjectConst>();
    const char* step_id = stringOrNull(step_obj["id"]);
    if (step_id == nullptr) {
      step_id = stringOrNull(step_obj["step_id"]);
    }
    if (step_id == nullptr) {
      step_id = stringOrNull(step_obj["stepId"]);
    }
    if (step_id == nullptr) {
      continue;
    }

    const char* const screen_keys[] = {"screen_scene_id", "screenSceneId"};
    const char* screen_scene_id =
        readScenarioField(variant, screen_keys, sizeof(screen_keys) / sizeof(screen_keys[0]));
    if (screen_scene_id == nullptr) {
      screen_scene_id = readScenarioField(
          step_obj["resources"], screen_keys, sizeof(screen_keys) / sizeof(screen_keys[0]));
    }

    const char* const audio_keys[] = {"audio_pack_id", "audioPackId"};
    const char* audio_pack_id =
        readScenarioField(variant, audio_keys, sizeof(audio_keys) / sizeof(audio_keys[0]));
    if (audio_pack_id == nullptr) {
      audio_pack_id = readScenarioField(step_obj["resources"],
                                        audio_keys,
                                        sizeof(audio_keys) / sizeof(audio_keys[0]));
    }

    JsonArrayConst action_ids = step_obj["action_ids"].as<JsonArrayConst>();
    if (action_ids.isNull()) {
      action_ids = step_obj["actionIds"].as<JsonArrayConst>();
    }
    if (action_ids.isNull()) {
      action_ids = step_obj["actions"].as<JsonArrayConst>();
    }
    if (action_ids.isNull()) {
      action_ids = step_obj["resources"]["action_ids"].as<JsonArrayConst>();
    }
    if (action_ids.isNull()) {
      action_ids = step_obj["resources"]["actionIds"].as<JsonArrayConst>();
    }
    const bool has_action_override = !action_ids.isNull() && action_ids.size() > 0U;

    if (screen_scene_id == nullptr && audio_pack_id == nullptr && !has_action_override) {
      continue;
    }
    if (step_resource_override_count_ >= kMaxStepResourceOverrides) {
      Serial.printf("[SCENARIO] step overrides truncated at %u entries\n", kMaxStepResourceOverrides);
      break;
    }

    StepResourceOverride& entry = step_resource_overrides_[step_resource_override_count_++];
    entry.step_id = step_id;
    entry.screen_scene_id = (screen_scene_id != nullptr) ? screen_scene_id : "";
    entry.audio_pack_id = (audio_pack_id != nullptr) ? audio_pack_id : "";
    entry.action_count = 0U;
    for (uint8_t action_index = 0; action_index < StepResourceOverride::kMaxActionOverrides; ++action_index) {
      entry.action_ids[action_index].remove(0);
      entry.action_ptrs[action_index] = nullptr;
    }
    if (has_action_override) {
      for (JsonVariantConst action_id_variant : action_ids) {
        if (entry.action_count >= StepResourceOverride::kMaxActionOverrides) {
          break;
        }
        if (!action_id_variant.is<const char*>()) {
          continue;
        }
        const char* action_id = action_id_variant.as<const char*>();
        if (action_id == nullptr || action_id[0] == '\0') {
          continue;
        }
        entry.action_ids[entry.action_count] = action_id;
        entry.action_ptrs[entry.action_count] = entry.action_ids[entry.action_count].c_str();
        ++entry.action_count;
      }
    }
  }

  if (step_resource_override_count_ > 0U) {
    Serial.printf("[SCENARIO] loaded %u step resource overrides\n", step_resource_override_count_);
  }
}

const ScenarioManager::StepResourceOverride* ScenarioManager::findStepResourceOverride(const char* step_id) const {
  if (step_id == nullptr || step_id[0] == '\0') {
    return nullptr;
  }
  for (uint8_t index = 0; index < step_resource_override_count_; ++index) {
    const StepResourceOverride& candidate = step_resource_overrides_[index];
    if (candidate.step_id == step_id) {
      return &candidate;
    }
  }
  return nullptr;
}

void ScenarioManager::applyStepResourceOverride(const StepDef* step,
                                                const char** out_screen_scene_id,
                                                const char** out_audio_pack_id,
                                                const char* const** out_action_ids,
                                                uint8_t* out_action_count) const {
  if (step == nullptr) {
    return;
  }
  const StepResourceOverride* entry = findStepResourceOverride(step->id);
  if (entry == nullptr) {
    return;
  }
  if (out_screen_scene_id != nullptr && !entry->screen_scene_id.isEmpty()) {
    *out_screen_scene_id = entry->screen_scene_id.c_str();
  }
  if (out_audio_pack_id != nullptr && !entry->audio_pack_id.isEmpty()) {
    *out_audio_pack_id = entry->audio_pack_id.c_str();
  }
  if (out_action_ids != nullptr && out_action_count != nullptr && entry->action_count > 0U) {
    *out_action_ids = entry->action_ptrs;
    *out_action_count = entry->action_count;
  }
}
