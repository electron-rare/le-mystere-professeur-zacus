#include "la_detector_app.h"

bool LaDetectorApp::begin(const StoryAppContext& context) {
  context_ = context;
  snapshot_ = {};
  snapshot_.status = "READY";
  return true;
}

void LaDetectorApp::start(const StoryStepContext& stepContext) {
  snapshot_.bindingId = (stepContext.binding != nullptr) ? stepContext.binding->id : nullptr;
  snapshot_.active = true;
  snapshot_.status = "RUNNING";
  snapshot_.startedAtMs = stepContext.nowMs;
}

void LaDetectorApp::update(uint32_t nowMs, const StoryEventSink& sink) {
  (void)nowMs;
  (void)sink;
}

void LaDetectorApp::stop(const char* reason) {
  snapshot_.active = false;
  snapshot_.status = (reason != nullptr && reason[0] != '\0') ? reason : "STOPPED";
}

bool LaDetectorApp::handleEvent(const StoryEvent& event, const StoryEventSink& sink) {
  (void)sink;
  if (!snapshot_.active) {
    return false;
  }
  if (event.type == StoryEventType::kUnlock) {
    snapshot_.status = "UNLOCK_EVENT";
    return true;
  }
  return false;
}

StoryAppSnapshot LaDetectorApp::snapshot() const {
  return snapshot_;
}
