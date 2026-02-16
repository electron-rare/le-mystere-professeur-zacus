#include "story_controller_v2.h"

#include <cstring>

#include "../../story/generated/scenarios_gen.h"

namespace {

constexpr uint8_t kStormQueueThreshold = 8U;
constexpr uint32_t kStormDuplicateWindowMs = 40U;

void safeCopy(char* out, size_t outLen, const char* in) {
  if (out == nullptr || outLen == 0U) {
    return;
  }
  out[0] = '\0';
  if (in == nullptr || in[0] == '\0') {
    return;
  }
  snprintf(out, outLen, "%s", in);
}

bool sameText(const char* lhs, const char* rhs) {
  if (lhs == nullptr || rhs == nullptr) {
    return false;
  }
  return strcmp(lhs, rhs) == 0;
}

bool sameEventSignature(const StoryEvent& lhs, const StoryEvent& rhs) {
  return lhs.type == rhs.type && lhs.value == rhs.value && sameText(lhs.name, rhs.name);
}

const char* traceLevelLabelLocal(StoryControllerV2::TraceLevel level) {
  switch (level) {
    case StoryControllerV2::TraceLevel::kErr:
      return "ERR";
    case StoryControllerV2::TraceLevel::kInfo:
      return "INFO";
    case StoryControllerV2::TraceLevel::kDebug:
      return "DEBUG";
    case StoryControllerV2::TraceLevel::kOff:
    default:
      return "OFF";
  }
}

}  // namespace

StoryControllerV2::StoryControllerV2(AudioService& audio, const Hooks& hooks, const Options& options)
    : audio_(audio),
      hooks_(hooks),
      options_(options),
      testDelayMs_(options.etape2TestDelayMs) {}

bool StoryControllerV2::begin(uint32_t nowMs) {
  return setScenario(options_.defaultScenarioId, nowMs, "boot");
}

bool StoryControllerV2::setScenario(const char* scenarioId, uint32_t nowMs, const char* source) {
  const char* id = (scenarioId != nullptr && scenarioId[0] != '\0') ? scenarioId : options_.defaultScenarioId;
  const ScenarioDef* scenario = generatedScenarioById(id);
  if (scenario == nullptr) {
    Serial.printf("[STORY_V2] scenario not found id=%s (%s)\n", id, source);
    return false;
  }

  if (!initialized_) {
    StoryAppContext appContext = {};
    appContext.audioService = &audio_;
    appContext.startRandomTokenBase = hooks_.startRandomTokenBase;
    appContext.startFallbackBaseFx = hooks_.startFallbackBaseFx;
    appContext.applyAction = hooks_.applyAction;
    appContext.laRuntime = hooks_.laRuntime;
    appContext.onUnlockRuntimeApplied = hooks_.onUnlockRuntimeApplied;
    if (!appHost_.begin(appContext)) {
      Serial.printf("[STORY_V2] app host begin failed (%s)\n", source);
      return false;
    }
  }

  StoryAppValidation appValidation = {};
  if (!appHost_.validateScenario(*scenario, &appValidation)) {
    Serial.printf("[STORY_V2] scenario app validation failed code=%s detail=%s\n",
                  appValidation.code != nullptr ? appValidation.code : "-",
                  appValidation.detail != nullptr ? appValidation.detail : "-");
    return false;
  }

  if (!engine_.loadScenario(*scenario)) {
    return false;
  }
  if (!engine_.start(scenario->id, nowMs)) {
    return false;
  }

  safeCopy(scenarioId_, sizeof(scenarioId_), scenario->id);
  paused_ = false;
  pausedAtMs_ = 0U;
  resetRuntimeState();
  applyCurrentStep(nowMs, "start");
  initialized_ = true;
  Serial.printf("[STORY_V2] scenario set id=%s via=%s\n", scenarioId_, source);
  return true;
}

void StoryControllerV2::reset(uint32_t nowMs, const char* source) {
  if (!initialized_) {
    begin(nowMs);
    return;
  }
  if (!engine_.start(scenarioId_[0] != '\0' ? scenarioId_ : options_.defaultScenarioId, nowMs)) {
    return;
  }
  paused_ = false;
  pausedAtMs_ = 0U;
  resetRuntimeState();
  applyCurrentStep(nowMs, source);
  Serial.printf("[STORY_V2] reset via=%s\n", source);
}

void StoryControllerV2::onUnlock(uint32_t nowMs, const char* source) {
  if (!initialized_ && !begin(nowMs)) {
    return;
  }
  postEvent(StoryEventType::kUnlock, "UNLOCK", 1, nowMs, source);
  update(nowMs);
}

void StoryControllerV2::update(uint32_t nowMs) {
  if (!initialized_) {
    return;
  }
  if (paused_) {
    return;
  }

  appHost_.update(nowMs, makeAppEventSink("app_update"));
  const StorySnapshot beforeTick = engine_.snapshot();
  if (beforeTick.queuedEvents > maxQueueDepth_) {
    maxQueueDepth_ = beforeTick.queuedEvents;
  }

  const char* currentStep = stepId();
  if (currentStep != nullptr && sameText(currentStep, options_.waitEtape2StepId) &&
      etape2DueMs_ != 0U && !etape2DuePosted_ &&
      static_cast<int32_t>(nowMs - etape2DueMs_) >= 0) {
    etape2DuePosted_ = true;
    postEvent(StoryEventType::kTimer, options_.timerEventName, 1, nowMs, "timer_due");
  }

  engine_.update(nowMs);
  if (engine_.consumeStepChanged()) {
    ++transitionCount_;
    applyCurrentStep(nowMs, "step_changed");
  }
  const StorySnapshot afterTick = engine_.snapshot();
  if (afterTick.queuedEvents > maxQueueDepth_) {
    maxQueueDepth_ = afterTick.queuedEvents;
  }
}

bool StoryControllerV2::pause(uint32_t nowMs, const char* source) {
  const StorySnapshot snap = engine_.snapshot();
  if (!snap.running || paused_) {
    return false;
  }
  paused_ = true;
  pausedAtMs_ = nowMs;
  Serial.printf("[STORY_V2] pause via=%s\n", source != nullptr ? source : "-");
  return true;
}

bool StoryControllerV2::resume(uint32_t nowMs, const char* source) {
  if (!paused_) {
    return false;
  }
  paused_ = false;
  Serial.printf("[STORY_V2] resume via=%s\n", source != nullptr ? source : "-");
  (void)nowMs;
  return true;
}

bool StoryControllerV2::skipToNextStep(uint32_t nowMs,
                                       const char* source,
                                       const char** outPrevStep,
                                       const char** outNextStep) {
  if (outPrevStep != nullptr) {
    *outPrevStep = nullptr;
  }
  if (outNextStep != nullptr) {
    *outNextStep = nullptr;
  }
  if (!initialized_ || paused_) {
    return false;
  }
  const ScenarioDef* scenario = engine_.scenario();
  const StorySnapshot snap = engine_.snapshot();
  if (scenario == nullptr || !snap.running || snap.stepIndex >= scenario->stepCount) {
    return false;
  }

  const StepDef& step = scenario->steps[snap.stepIndex];
  if (step.transitionCount == 0U) {
    return false;
  }

  uint8_t selected = 0U;
  uint8_t selectedPriority = 0U;
  for (uint8_t i = 0U; i < step.transitionCount; ++i) {
    const TransitionDef& transition = step.transitions[i];
    if (i == 0U || transition.priority > selectedPriority) {
      selected = i;
      selectedPriority = transition.priority;
    }
  }

  const TransitionDef& transition = step.transitions[selected];
  if (transition.targetStepId == nullptr || transition.targetStepId[0] == '\0') {
    return false;
  }
  if (outPrevStep != nullptr) {
    *outPrevStep = step.id;
  }
  if (!engine_.jumpToStep(transition.targetStepId,
                          transition.id != nullptr ? transition.id : source,
                          nowMs)) {
    return false;
  }
  ++transitionCount_;
  applyCurrentStep(nowMs, source);
  if (outNextStep != nullptr) {
    *outNextStep = stepId();
  }
  return true;
}

bool StoryControllerV2::isPaused() const {
  return paused_;
}

bool StoryControllerV2::isRunning() const {
  return engine_.snapshot().running && !paused_;
}

bool StoryControllerV2::isMp3GateOpen() const {
  const StorySnapshot snap = engine_.snapshot();
  if (!snap.running) {
    return true;
  }
  return snap.mp3GateOpen;
}

void StoryControllerV2::forceEtape2DueNow(uint32_t nowMs, const char* source) {
  postEvent(StoryEventType::kSerial, "FORCE_ETAPE2", 1, nowMs, source);
  update(nowMs);
}

void StoryControllerV2::setTestMode(bool enabled, uint32_t nowMs, const char* source) {
  if (testMode_ == enabled) {
    Serial.printf("[STORY_V2] test mode unchanged=%u (%s)\n", testMode_ ? 1U : 0U, source);
    return;
  }
  testMode_ = enabled;
  if (sameText(stepId(), options_.waitEtape2StepId)) {
    etape2DueMs_ = nowMs + activeDelayMs();
    etape2DuePosted_ = false;
  }
  Serial.printf("[STORY_V2] test mode=%u delay=%lu ms (%s)\n",
                testMode_ ? 1U : 0U,
                static_cast<unsigned long>(activeDelayMs()),
                source);
}

void StoryControllerV2::setTestDelayMs(uint32_t delayMs, uint32_t nowMs, const char* source) {
  if (delayMs < 100U) {
    delayMs = 100U;
  } else if (delayMs > 300000U) {
    delayMs = 300000U;
  }
  testDelayMs_ = delayMs;
  if (testMode_ && sameText(stepId(), options_.waitEtape2StepId)) {
    etape2DueMs_ = nowMs + activeDelayMs();
    etape2DuePosted_ = false;
  }
  Serial.printf("[STORY_V2] test delay=%lu ms (%s)\n",
                static_cast<unsigned long>(testDelayMs_),
                source);
}

bool StoryControllerV2::jumpToStep(const char* stepId, uint32_t nowMs, const char* source) {
  if (!initialized_) {
    return false;
  }
  const bool ok = engine_.jumpToStep(stepId, source, nowMs);
  if (ok) {
    ++transitionCount_;
    applyCurrentStep(nowMs, source);
  }
  return ok;
}

bool StoryControllerV2::postSerialEvent(const char* eventName, uint32_t nowMs, const char* source) {
  if (eventName == nullptr || eventName[0] == '\0') {
    return false;
  }
  const bool ok = postEvent(StoryEventType::kSerial, eventName, 1, nowMs, source);
  if (ok) {
    update(nowMs);
  }
  return ok;
}

void StoryControllerV2::printStatus(uint32_t nowMs, const char* source) const {
  const StorySnapshot snap = engine_.snapshot();
  uint32_t leftMs = 0U;
  if (etape2DueMs_ != 0U && static_cast<int32_t>(etape2DueMs_ - nowMs) > 0) {
    leftMs = etape2DueMs_ - nowMs;
  }
  Serial.printf(
      "[STORY_V2] STATUS via=%s run=%u scenario=%s step=%s prev=%s gate=%u test=%u delay=%lu left=%lu queue=%u screen=%s err=%s\n",
      source,
      snap.running ? 1U : 0U,
      snap.scenarioId != nullptr ? snap.scenarioId : "-",
      snap.stepId != nullptr ? snap.stepId : "-",
      snap.previousStepId != nullptr ? snap.previousStepId : "-",
      snap.mp3GateOpen ? 1U : 0U,
      testMode_ ? 1U : 0U,
      static_cast<unsigned long>(activeDelayMs()),
      static_cast<unsigned long>(leftMs),
      static_cast<unsigned int>(snap.queuedEvents),
      activeScreenSceneId_[0] != '\0' ? activeScreenSceneId_ : "-",
      engine_.lastError());
}

StoryControllerV2::StoryControllerV2Snapshot StoryControllerV2::snapshot(bool enabled,
                                                                          uint32_t nowMs) const {
  StoryControllerV2Snapshot out = {};
  const StorySnapshot snap = engine_.snapshot();
  out.enabled = enabled;
  out.running = snap.running;
  out.paused = paused_;
  out.scenarioId = snap.scenarioId;
  out.stepId = snap.stepId;
  out.mp3GateOpen = snap.mp3GateOpen;
  out.queueDepth = snap.queuedEvents;
  out.appHostError = appHost_.lastError();
  out.engineError = engine_.lastError();
  out.etape2DueMs = etape2DueMs_;
  out.testMode = testMode_;
  (void)nowMs;
  return out;
}

StoryControllerV2::StoryMetricsSnapshot StoryControllerV2::metricsSnapshot() const {
  StoryMetricsSnapshot out = {};
  out.eventsPosted = postedEventsCount_;
  out.eventsAccepted = acceptedEventsCount_;
  out.eventsRejected = rejectedEventsCount_;
  out.stormDropped = droppedStormEvents_;
  out.queueDropped = engine_.droppedEvents();
  out.transitions = transitionCount_;
  out.maxQueueDepth = maxQueueDepth_;
  out.lastAppHostError = appHost_.lastError();
  out.lastEngineError = engine_.lastError();
  return out;
}

void StoryControllerV2::resetMetrics() {
  postedEventsCount_ = 0U;
  acceptedEventsCount_ = 0U;
  rejectedEventsCount_ = 0U;
  droppedStormEvents_ = 0U;
  transitionCount_ = 0U;
  maxQueueDepth_ = 0U;
}

const char* StoryControllerV2::healthLabel(bool enabled, uint32_t nowMs) const {
  const StoryControllerV2Snapshot snap = snapshot(enabled, nowMs);
  if (!snap.enabled) {
    return "OUT_OF_CONTEXT";
  }
  const bool hasEngineError =
      snap.engineError != nullptr && snap.engineError[0] != '\0' && strcmp(snap.engineError, "OK") != 0;
  const bool hasAppError =
      snap.appHostError != nullptr && snap.appHostError[0] != '\0' && strcmp(snap.appHostError, "OK") != 0;
  if (hasEngineError || hasAppError) {
    return "ERROR";
  }
  if (snap.running && (snap.queueDepth > 0U || audio_.isBaseBusy())) {
    return "BUSY";
  }
  return "OK";
}

void StoryControllerV2::setTraceEnabled(bool enabled) {
  setTraceLevel(enabled ? TraceLevel::kDebug : TraceLevel::kOff);
}

bool StoryControllerV2::traceEnabled() const {
  return traceLevel_ != TraceLevel::kOff;
}

bool StoryControllerV2::setTraceLevel(TraceLevel level) {
  if (traceLevel_ == level) {
    return false;
  }
  traceLevel_ = level;
  Serial.printf("[STORY_V2] trace_level=%s\n", traceLevelLabelLocal(traceLevel_));
  return true;
}

StoryControllerV2::TraceLevel StoryControllerV2::traceLevel() const {
  return traceLevel_;
}

const char* StoryControllerV2::traceLevelLabel() const {
  return traceLevelLabel(traceLevel_);
}

const char* StoryControllerV2::traceLevelLabel(TraceLevel level) {
  return traceLevelLabelLocal(level);
}

void StoryControllerV2::printScenarioList(const char* source) const {
  Serial.printf("[STORY_V2] LIST via=%s count=%u\n",
                source != nullptr ? source : "-",
                static_cast<unsigned int>(generatedScenarioCount()));
  for (uint8_t i = 0U; i < generatedScenarioCount(); ++i) {
    const char* id = generatedScenarioIdAt(i);
    Serial.printf("[STORY_V2] LIST[%u]=%s\n",
                  static_cast<unsigned int>(i),
                  id != nullptr ? id : "-");
  }
}

bool StoryControllerV2::validateActiveScenario(const char* source) const {
  const ScenarioDef* scenario = engine_.scenario();
  if (scenario == nullptr) {
    Serial.printf("[STORY_V2] VALIDATE via=%s code=NO_SCENARIO\n", source != nullptr ? source : "-");
    return false;
  }

  StoryValidationError coreError = {};
  if (!storyValidateScenarioDef(*scenario, &coreError)) {
    Serial.printf("[STORY_V2] VALIDATE via=%s code=%s detail=%s\n",
                  source != nullptr ? source : "-",
                  coreError.code != nullptr ? coreError.code : "-",
                  coreError.detail != nullptr ? coreError.detail : "-");
    return false;
  }

  StoryAppValidation appValidation = {};
  if (!appHost_.validateScenario(*scenario, &appValidation)) {
    Serial.printf("[STORY_V2] VALIDATE via=%s code=%s detail=%s\n",
                  source != nullptr ? source : "-",
                  appValidation.code != nullptr ? appValidation.code : "-",
                  appValidation.detail != nullptr ? appValidation.detail : "-");
    return false;
  }

  Serial.printf("[STORY_V2] VALIDATE via=%s code=OK scenario=%s\n",
                source != nullptr ? source : "-",
                scenario->id != nullptr ? scenario->id : "-");
  return true;
}

const char* StoryControllerV2::scenarioId() const {
  return scenarioId_[0] != '\0' ? scenarioId_ : nullptr;
}

const char* StoryControllerV2::stepId() const {
  const StorySnapshot snap = engine_.snapshot();
  return snap.stepId;
}

const char* StoryControllerV2::activeScreenSceneId() const {
  return (activeScreenSceneId_[0] != '\0') ? activeScreenSceneId_ : nullptr;
}

const char* StoryControllerV2::lastError() const {
  return engine_.lastError();
}

const char* StoryControllerV2::lastTransitionId() const {
  return engine_.lastTransitionId();
}

const ScenarioDef* StoryControllerV2::scenario() const {
  return engine_.scenario();
}

bool StoryControllerV2::postEvent(StoryEventType type,
                                  const char* eventName,
                                  int32_t value,
                                  uint32_t nowMs,
                                  const char* source) {
  StoryEvent event = {};
  event.type = type;
  event.value = value;
  event.atMs = nowMs;
  safeCopy(event.name, sizeof(event.name), eventName);
  return postEventInternal(event, source, true);
}

bool StoryControllerV2::postEventInternal(const StoryEvent& event,
                                          const char* source,
                                          bool notifyApps) {
  ++postedEventsCount_;
  if (isDuplicateStormEvent(event)) {
    ++droppedStormEvents_;
    ++rejectedEventsCount_;
    if (traceLevel_ == TraceLevel::kDebug) {
      Serial.printf("[STORY_V2][TRACE] drop storm event type=%u name=%s queue=%u dropped=%lu\n",
                    static_cast<unsigned int>(event.type),
                    event.name,
                    static_cast<unsigned int>(engine_.snapshot().queuedEvents),
                    static_cast<unsigned long>(droppedStormEvents_));
    }
    return false;
  }

  if (notifyApps) {
    appHost_.handleEvent(event, makeAppEventSink(source));
  }

  const bool ok = engine_.postEvent(event);
  if (!ok) {
    ++rejectedEventsCount_;
    Serial.printf("[STORY_V2] postEvent failed type=%u name=%s (%s)\n",
                  static_cast<unsigned int>(event.type),
                  event.name,
                  source != nullptr ? source : "-");
  } else {
    ++acceptedEventsCount_;
    lastPostedEvent_ = event;
    hasLastPostedEvent_ = true;
    lastPostedEventAtMs_ = event.atMs;
    const StorySnapshot snap = engine_.snapshot();
    if (snap.queuedEvents > maxQueueDepth_) {
      maxQueueDepth_ = snap.queuedEvents;
    }
    if (traceLevel_ == TraceLevel::kDebug) {
      Serial.printf("[STORY_V2][TRACE] post event type=%u name=%s via=%s queue=%u\n",
                    static_cast<unsigned int>(event.type),
                    event.name,
                    source != nullptr ? source : "-",
                    static_cast<unsigned int>(snap.queuedEvents));
    }
  }
  return ok;
}

void StoryControllerV2::applyCurrentStep(uint32_t nowMs, const char* source) {
  const StepDef* step = engine_.currentStep();
  if (step == nullptr) {
    safeCopy(activeScreenSceneId_, sizeof(activeScreenSceneId_), nullptr);
    appHost_.stopAll("no_step");
    return;
  }

  etape2DueMs_ = 0U;
  etape2DuePosted_ = false;

  const ScenarioDef* scenario = engine_.scenario();
  if (!appHost_.startStep(scenario, step, nowMs, source)) {
    Serial.printf("[STORY_V2] app host startStep failed err=%s\n", appHost_.lastError());
  }

  const char* sceneId = appHost_.activeScreenSceneId();
  if (sceneId == nullptr || sceneId[0] == '\0') {
    sceneId = step->resources.screenSceneId;
  }
  safeCopy(activeScreenSceneId_, sizeof(activeScreenSceneId_), sceneId);

  if (sameText(step->id, options_.waitEtape2StepId)) {
    etape2DueMs_ = nowMs + activeDelayMs();
    etape2DuePosted_ = false;
    Serial.printf("[STORY_V2] etape2 timer armed in %lu ms\n",
                  static_cast<unsigned long>(activeDelayMs()));
  }
}

uint32_t StoryControllerV2::activeDelayMs() const {
  return testMode_ ? testDelayMs_ : options_.etape2DelayMs;
}

void StoryControllerV2::resetRuntimeState() {
  etape2DueMs_ = 0U;
  etape2DuePosted_ = false;
  safeCopy(activeScreenSceneId_, sizeof(activeScreenSceneId_), nullptr);
  hasLastPostedEvent_ = false;
  lastPostedEventAtMs_ = 0U;
  resetMetrics();
  appHost_.stopAll("reset");
}

StoryEventSink StoryControllerV2::makeAppEventSink(const char* source) {
  (void)source;
  StoryEventSink sink = {};
  sink.postFn = &StoryControllerV2::postEventFromSink;
  sink.user = this;
  return sink;
}

bool StoryControllerV2::postEventFromSink(const StoryEvent& event, void* user) {
  StoryControllerV2* self = static_cast<StoryControllerV2*>(user);
  if (self == nullptr) {
    return false;
  }
  return self->postEventInternal(event, "app_sink", false);
}

bool StoryControllerV2::isDuplicateStormEvent(const StoryEvent& event) const {
  if (!hasLastPostedEvent_) {
    return false;
  }
  if (engine_.snapshot().queuedEvents < kStormQueueThreshold) {
    return false;
  }
  if (!sameEventSignature(event, lastPostedEvent_)) {
    return false;
  }
  return static_cast<uint32_t>(event.atMs - lastPostedEventAtMs_) <= kStormDuplicateWindowMs;
}
