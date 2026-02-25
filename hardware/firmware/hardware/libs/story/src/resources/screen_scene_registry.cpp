#include "screen_scene_registry.h"

#include <cstring>

namespace {

struct SceneAliasDef {
  const char* alias;
  const char* canonical;
};

// Canonical scene ids accepted by the runtime.
constexpr ScreenSceneDef kScenes[] = {
    {"SCENE_LOCKED", 0U, 0U},
    {"SCENE_BROKEN", 0U, 0U},
    {"SCENE_SEARCH", 1U, 1U},
    {"SCENE_LA_DETECTOR", 1U, 1U},
    {"SCENE_CAMERA_SCAN", 1U, 1U},
    {"SCENE_SIGNAL_SPIKE", 1U, 2U},
    {"SCENE_REWARD", 1U, 2U},
    {"SCENE_MEDIA_ARCHIVE", 2U, 2U},
    {"SCENE_READY", 2U, 2U},
    {"SCENE_WIN", 1U, 2U},
    {"SCENE_WINNER", 1U, 2U},
    {"SCENE_FIREWORKS", 1U, 2U},
    {"SCENE_WIN_ETAPE", 1U, 2U},
    {"SCENE_MP3_PLAYER", 1U, 2U},
    {"SCENE_MEDIA_MANAGER", 1U, 2U},
    {"SCENE_PHOTO_MANAGER", 1U, 2U},
};

// Controlled legacy aliases used during the migration window.
constexpr SceneAliasDef kSceneAliases[] = {
    {"SCENE_LA_DETECT", "SCENE_LA_DETECTOR"},
    {"SCENE_LOCK", "SCENE_LOCKED"},
    {"LOCKED", "SCENE_LOCKED"},
    {"LOCK", "SCENE_LOCKED"},
    {"SCENE_AUDIO_PLAYER", "SCENE_MP3_PLAYER"},
    {"SCENE_MP3", "SCENE_MP3_PLAYER"},
};

const ScreenSceneDef* findScene(const char* sceneId) {
  for (const ScreenSceneDef& scene : kScenes) {
    if (std::strcmp(scene.id, sceneId) == 0) {
      return &scene;
    }
  }
  return nullptr;
}

const char* normalizeAlias(const char* sceneId) {
  for (const SceneAliasDef& alias : kSceneAliases) {
    if (std::strcmp(alias.alias, sceneId) == 0) {
      return alias.canonical;
    }
  }
  return nullptr;
}

}  // namespace

const char* storyNormalizeScreenSceneId(const char* sceneId) {
  if (sceneId == nullptr || sceneId[0] == '\0') {
    return nullptr;
  }
  const ScreenSceneDef* found = findScene(sceneId);
  if (found == nullptr) {
    const char* normalized_alias = normalizeAlias(sceneId);
    if (normalized_alias != nullptr && findScene(normalized_alias) != nullptr) {
      return normalized_alias;
    }
    return nullptr;
  }
  return sceneId;
}

const ScreenSceneDef* storyFindScreenScene(const char* sceneId) {
  if (sceneId == nullptr || sceneId[0] == '\0') {
    return nullptr;
  }
  const char* normalized = storyNormalizeScreenSceneId(sceneId);
  if (normalized == nullptr) {
    return nullptr;
  }
  return findScene(normalized);
}
