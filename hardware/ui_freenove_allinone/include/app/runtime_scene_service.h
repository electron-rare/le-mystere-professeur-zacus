// runtime_scene_service.h - scene refresh + audio kick bridge.
#pragma once

#include <Arduino.h>

class RuntimeSceneService {
 public:
  using RefreshSceneFn = void (*)(bool force_render);
  using StartPendingAudioFn = void (*)();

  void configure(RefreshSceneFn refresh_scene, StartPendingAudioFn start_pending_audio);
  void refreshSceneIfNeeded(bool force_render) const;
  void startPendingAudioIfAny() const;

 private:
  RefreshSceneFn refresh_scene_ = nullptr;
  StartPendingAudioFn start_pending_audio_ = nullptr;
};
