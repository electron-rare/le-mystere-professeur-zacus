#include "mp3_gate_app.h"

bool Mp3GateApp::begin(const StoryAppContext& context) {
  context_ = context;
  snapshot_ = {};
  snapshot_.status = "READY";
  gateOpen_ = true;
  return true;
}

void Mp3GateApp::start(const StoryStepContext& stepContext) {
  snapshot_.bindingId = (stepContext.binding != nullptr) ? stepContext.binding->id : nullptr;
  snapshot_.active = true;
  snapshot_.status = "RUNNING";
  snapshot_.startedAtMs = stepContext.nowMs;
  gateOpen_ = (stepContext.step != nullptr) ? stepContext.step->mp3GateOpen : true;
}

void Mp3GateApp::update(uint32_t nowMs, const StoryEventSink& sink) {
  (void)nowMs;
  (void)sink;
}

void Mp3GateApp::stop(const char* reason) {
  snapshot_.active = false;
  snapshot_.status = (reason != nullptr && reason[0] != '\0') ? reason : "STOPPED";
}

bool Mp3GateApp::handleEvent(const StoryEvent& event, const StoryEventSink& sink) {
  (void)event;
  (void)sink;
  return false;
}

StoryAppSnapshot Mp3GateApp::snapshot() const {
  return snapshot_;
}

bool Mp3GateApp::gateOpen() const {
  return gateOpen_;
}
