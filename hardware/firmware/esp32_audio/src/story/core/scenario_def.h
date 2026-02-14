#pragma once

#include <Arduino.h>

enum class StoryEventType : uint8_t {
  kNone = 0,
  kUnlock,
  kAudioDone,
  kTimer,
  kSerial,
  kAction,
};

enum class StoryTransitionTrigger : uint8_t {
  kOnEvent = 0,
  kAfterMs,
  kImmediate,
};

struct StoryEvent {
  StoryEventType type;
  char name[24];
  int32_t value;
  uint32_t atMs;
};

struct StoryValidationError {
  const char* code;
  const char* detail;
};

enum class StoryAppType : uint8_t {
  kNone = 0,
  kLaDetector,
  kAudioPack,
  kScreenScene,
  kMp3Gate,
};

struct AppBindingDef {
  const char* id;
  StoryAppType type;
};

struct ResourceBindings {
  const char* screenSceneId;
  const char* audioPackId;
  const char* const* actionIds;
  uint8_t actionCount;
  const char* const* appIds;
  uint8_t appCount;
};

struct TransitionDef {
  const char* id;
  StoryTransitionTrigger trigger;
  StoryEventType eventType;
  const char* eventName;
  uint32_t afterMs;
  const char* targetStepId;
  uint8_t priority;
};

struct StepDef {
  const char* id;
  ResourceBindings resources;
  const TransitionDef* transitions;
  uint8_t transitionCount;
  bool mp3GateOpen;
};

struct ScenarioDef {
  const char* id;
  uint16_t version;
  const StepDef* steps;
  uint8_t stepCount;
  const char* initialStepId;
};

struct StorySnapshot {
  bool running;
  bool mp3GateOpen;
  const char* scenarioId;
  const char* stepId;
  const char* previousStepId;
  uint8_t stepIndex;
  uint32_t enteredAtMs;
  uint32_t nextDueAtMs;
  uint8_t queuedEvents;
};

int8_t storyFindStepIndex(const ScenarioDef& scenario, const char* stepId);
bool storyValidateScenarioDef(const ScenarioDef& scenario, StoryValidationError* outError);
