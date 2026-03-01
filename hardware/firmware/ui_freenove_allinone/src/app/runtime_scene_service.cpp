#include "app/runtime_scene_service.h"

void RuntimeSceneService::configure(RefreshSceneFn refresh_scene, StartPendingAudioFn start_pending_audio) {
  refresh_scene_ = refresh_scene;
  start_pending_audio_ = start_pending_audio;
}

void RuntimeSceneService::refreshSceneIfNeeded(bool force_render) const {
  if (refresh_scene_ == nullptr) {
    return;
  }
  refresh_scene_(force_render);
}

void RuntimeSceneService::startPendingAudioIfAny() const {
  if (start_pending_audio_ == nullptr) {
    return;
  }
  start_pending_audio_();
}
