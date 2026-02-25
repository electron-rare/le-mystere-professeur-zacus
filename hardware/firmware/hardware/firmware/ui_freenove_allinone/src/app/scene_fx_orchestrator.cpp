#include "app/scene_fx_orchestrator.h"

#include <cstring>

namespace {

bool isAmpSceneId(const char* scene_id) {
  if (scene_id == nullptr || scene_id[0] == '\0') {
    return false;
  }
  return std::strcmp(scene_id, "SCENE_MP3_PLAYER") == 0 ||
         std::strcmp(scene_id, "SCENE_AUDIO_PLAYER") == 0 ||
         std::strcmp(scene_id, "SCENE_MP3") == 0;
}

bool isCameraSceneId(const char* scene_id) {
  return (scene_id != nullptr && std::strcmp(scene_id, "SCENE_PHOTO_MANAGER") == 0);
}

bool isIntroSceneId(const char* scene_id) {
  return (scene_id != nullptr &&
          (std::strcmp(scene_id, "SCENE_WIN_ETAPE") == 0 ||
           std::strcmp(scene_id, "SCENE_WIN_ETAPE1") == 0 ||
           std::strcmp(scene_id, "SCENE_WIN_ETAPE2") == 0));
}

bool isDirectFxSceneId(const char* scene_id) {
  if (scene_id == nullptr || scene_id[0] == '\0') {
    return false;
  }
  return std::strcmp(scene_id, "SCENE_WINNER") == 0 || std::strcmp(scene_id, "SCENE_FIREWORKS") == 0;
}

}  // namespace

SceneTransitionPlan SceneFxOrchestrator::planTransition(const char* scene_id,
                                                        bool scene_changed,
                                                        bool force_refresh) {
  const char* target_scene = (scene_id != nullptr && scene_id[0] != '\0') ? scene_id : "SCENE_READY";
  const SceneRuntimeOwner target_owner = classifyOwner(target_scene);

  SceneTransitionPlan plan = {};
  plan.should_apply = scene_changed || force_refresh || !sameText(current_scene_id_, target_scene);
  plan.owner_changed = (target_owner != current_owner_);
  plan.scene_changed = scene_changed || !sameText(current_scene_id_, target_scene);
  plan.from_owner = current_owner_;
  plan.to_owner = target_owner;
  plan.from_scene = current_scene_id_;
  plan.to_scene = target_scene;
  return plan;
}

void SceneFxOrchestrator::applyTransition(const SceneTransitionPlan& plan) {
  if (!plan.should_apply || plan.to_scene == nullptr) {
    return;
  }
  current_owner_ = plan.to_owner;
  std::strncpy(current_scene_id_, plan.to_scene, sizeof(current_scene_id_) - 1U);
  current_scene_id_[sizeof(current_scene_id_) - 1U] = '\0';
}

SceneRuntimeOwner SceneFxOrchestrator::currentOwner() const {
  return current_owner_;
}

const char* SceneFxOrchestrator::currentSceneId() const {
  return current_scene_id_;
}

SceneRuntimeOwner SceneFxOrchestrator::classifyOwner(const char* scene_id) const {
  if (isCameraSceneId(scene_id)) {
    return SceneRuntimeOwner::kCamera;
  }
  if (isAmpSceneId(scene_id)) {
    return SceneRuntimeOwner::kAmp;
  }
  if (isIntroSceneId(scene_id)) {
    return SceneRuntimeOwner::kIntroFx;
  }
  if (isDirectFxSceneId(scene_id)) {
    return SceneRuntimeOwner::kDirectFx;
  }
  return SceneRuntimeOwner::kStory;
}

bool SceneFxOrchestrator::sameText(const char* lhs, const char* rhs) {
  if (lhs == nullptr || rhs == nullptr) {
    return false;
  }
  return std::strcmp(lhs, rhs) == 0;
}
