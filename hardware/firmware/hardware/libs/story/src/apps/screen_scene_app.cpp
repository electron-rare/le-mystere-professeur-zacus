#include "screen_scene_app.h"

bool ScreenSceneApp::begin(const StoryAppContext& context) {
  context_ = context;
  snapshot_ = {};
  snapshot_.status = "READY";
  activeSceneId_ = nullptr;
  return true;
}

void ScreenSceneApp::start(const StoryStepContext& stepContext) {
  snapshot_.bindingId = (stepContext.binding != nullptr) ? stepContext.binding->id : nullptr;
  snapshot_.active = true;
  snapshot_.status = "RUNNING";
  snapshot_.startedAtMs = stepContext.nowMs;
  activeSceneId_ = (stepContext.step != nullptr) ? stepContext.step->resources.screenSceneId : nullptr;
  if (activeSceneId_ == nullptr || activeSceneId_[0] == '\0') {
    snapshot_.status = "NO_SCENE";
  }
}

void ScreenSceneApp::update(uint32_t nowMs, const StoryEventSink& sink) {
  (void)nowMs;
  (void)sink;
}

void ScreenSceneApp::stop(const char* reason) {
  snapshot_.active = false;
  snapshot_.status = (reason != nullptr && reason[0] != '\0') ? reason : "STOPPED";
  activeSceneId_ = nullptr;
}

bool ScreenSceneApp::handleEvent(const StoryEvent& event, const StoryEventSink& sink) {
  (void)event;
  (void)sink;
  return false;
}

StoryAppSnapshot ScreenSceneApp::snapshot() const {
  return snapshot_;
}

const char* ScreenSceneApp::activeSceneId() const {
  return activeSceneId_;
}
