#include "screen_scene_registry.h"

#include <cstring>

namespace {

constexpr ScreenSceneDef kScenes[] = {
    {"SCENE_LOCKED", 0U, 0U},
    {"SCENE_SEARCH", 1U, 1U},
    {"SCENE_REWARD", 1U, 1U},
    {"SCENE_READY", 2U, 2U},
};

}  // namespace

const ScreenSceneDef* storyFindScreenScene(const char* sceneId) {
  if (sceneId == nullptr || sceneId[0] == '\0') {
    return nullptr;
  }
  for (const ScreenSceneDef& scene : kScenes) {
    if (scene.id != nullptr && strcmp(scene.id, sceneId) == 0) {
      return &scene;
    }
  }
  return nullptr;
}
