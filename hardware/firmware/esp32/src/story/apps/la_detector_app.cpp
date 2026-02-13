#include "la_detector_app.h"

#include <cstring>

#include "../generated/apps_gen.h"

bool LaDetectorApp::begin(const StoryAppContext& context) {
  context_ = context;
  snapshot_ = {};
  snapshot_.status = "READY";
  holdTargetMs_ = 3000U;
  requireListening_ = true;
  unlockPosted_ = false;
  snprintf(unlockEventName_, sizeof(unlockEventName_), "UNLOCK");
  return true;
}

void LaDetectorApp::start(const StoryStepContext& stepContext) {
  snapshot_.bindingId = (stepContext.binding != nullptr) ? stepContext.binding->id : nullptr;
  snapshot_.active = true;
  snapshot_.status = "RUNNING";
  snapshot_.startedAtMs = stepContext.nowMs;
  unlockPosted_ = false;
  loadConfigForBinding(snapshot_.bindingId);

  if (context_.laRuntime == nullptr) {
    snapshot_.status = "NO_RUNTIME";
    return;
  }

  LaDetectorRuntimeService::Config config = {};
  config.holdMs = holdTargetMs_;
  config.requireListening = requireListening_;
  config.unlockEventName = unlockEventName_;
  context_.laRuntime->start(config, stepContext.nowMs);
  snapshot_.status = "LISTENING";
}

void LaDetectorApp::update(uint32_t nowMs, const StoryEventSink& sink) {
  if (!snapshot_.active || context_.laRuntime == nullptr) {
    return;
  }

  context_.laRuntime->update(nowMs);
  const LaDetectorRuntimeService::Snapshot runtime = context_.laRuntime->snapshot();

  if (unlockPosted_) {
    snapshot_.status = "UNLOCK_SENT";
    return;
  }

  if (!runtime.active) {
    snapshot_.status = "IDLE";
    return;
  }
  if (!runtime.detectionEnabled) {
    snapshot_.status = "DETECT_OFF";
    return;
  }
  if (requireListening_ && !runtime.listening) {
    snapshot_.status = "WAIT_LISTEN";
    return;
  }
  if (runtime.detected) {
    snapshot_.status = "HOLDING";
  } else {
    snapshot_.status = "SEARCHING";
  }

  if (!context_.laRuntime->consumeUnlock()) {
    return;
  }

  if (context_.onUnlockRuntimeApplied != nullptr) {
    context_.onUnlockRuntimeApplied(nowMs, "story_app_la_unlock");
  }
  sink.emit(StoryEventType::kUnlock, unlockEventName_, 1, nowMs);
  unlockPosted_ = true;
  snapshot_.status = "UNLOCK_SENT";
}

void LaDetectorApp::stop(const char* reason) {
  if (context_.laRuntime != nullptr) {
    context_.laRuntime->stop((reason != nullptr && reason[0] != '\0') ? reason : "STOPPED");
  }
  snapshot_.active = false;
  snapshot_.status = (reason != nullptr && reason[0] != '\0') ? reason : "STOPPED";
  unlockPosted_ = false;
}

bool LaDetectorApp::handleEvent(const StoryEvent& event, const StoryEventSink& sink) {
  (void)sink;
  if (!snapshot_.active) {
    return false;
  }
  if (event.type == StoryEventType::kUnlock) {
    snapshot_.status = "UNLOCK_SEEN";
    return true;
  }
  return false;
}

StoryAppSnapshot LaDetectorApp::snapshot() const {
  return snapshot_;
}

void LaDetectorApp::loadConfigForBinding(const char* bindingId) {
  holdTargetMs_ = 3000U;
  requireListening_ = true;
  snprintf(unlockEventName_, sizeof(unlockEventName_), "UNLOCK");

  const LaDetectorAppConfigDef* config = generatedLaDetectorConfigByBindingId(bindingId);
  if (config == nullptr) {
    return;
  }

  if (config->holdMs >= 100U) {
    holdTargetMs_ = config->holdMs;
  }
  requireListening_ = config->requireListening;
  if (config->unlockEvent != nullptr && config->unlockEvent[0] != '\0') {
    snprintf(unlockEventName_, sizeof(unlockEventName_), "%s", config->unlockEvent);
  }
}
