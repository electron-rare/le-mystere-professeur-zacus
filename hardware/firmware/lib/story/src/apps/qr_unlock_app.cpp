#include "qr_unlock_app.h"

#include <cstdio>

bool QrUnlockApp::begin(const StoryAppContext& context) {
  (void)context;
  snapshot_.bindingId = binding_id_;
  snapshot_.active = false;
  snapshot_.status = "READY";
  snapshot_.startedAtMs = 0U;
  return true;
}

void QrUnlockApp::start(const StoryStepContext& stepContext) {
  const char* binding_id = (stepContext.binding != nullptr) ? stepContext.binding->id : "APP_QR_UNLOCK";
  std::snprintf(binding_id_, sizeof(binding_id_), "%s", (binding_id != nullptr) ? binding_id : "APP_QR_UNLOCK");
  snapshot_.bindingId = binding_id_;
  snapshot_.active = true;
  snapshot_.status = "ACTIVE";
  snapshot_.startedAtMs = stepContext.nowMs;
}

void QrUnlockApp::update(uint32_t nowMs, const StoryEventSink& sink) {
  (void)nowMs;
  (void)sink;
}

void QrUnlockApp::stop(const char* reason) {
  snapshot_.active = false;
  snapshot_.status = (reason != nullptr && reason[0] != '\0') ? reason : "STOPPED";
}

bool QrUnlockApp::handleEvent(const StoryEvent& event, const StoryEventSink& sink) {
  (void)event;
  (void)sink;
  return false;
}

StoryAppSnapshot QrUnlockApp::snapshot() const {
  return snapshot_;
}
