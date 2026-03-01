// scene_fx_orchestrator.h - explicit owner planning for scene runtime resources.
#pragma once

#include <Arduino.h>

enum class SceneRuntimeOwner : uint8_t {
  kStory = 0,
  kIntroFx,
  kDirectFx,
  kAmp,
  kCamera,
};

struct SceneTransitionPlan {
  bool should_apply = false;
  bool owner_changed = false;
  bool scene_changed = false;
  SceneRuntimeOwner from_owner = SceneRuntimeOwner::kStory;
  SceneRuntimeOwner to_owner = SceneRuntimeOwner::kStory;
  const char* from_scene = "";
  const char* to_scene = "";
};

class SceneFxOrchestrator {
 public:
  SceneTransitionPlan planTransition(const char* scene_id, bool scene_changed, bool force_refresh);
  void applyTransition(const SceneTransitionPlan& plan);
  SceneRuntimeOwner currentOwner() const;
  const char* currentSceneId() const;

 private:
  SceneRuntimeOwner classifyOwner(const char* scene_id) const;
  static bool sameText(const char* lhs, const char* rhs);

  SceneRuntimeOwner current_owner_ = SceneRuntimeOwner::kStory;
  char current_scene_id_[40] = "";
};

