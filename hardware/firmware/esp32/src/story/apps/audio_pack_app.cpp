#include "audio_pack_app.h"

#include <cstring>

#include "../resources/audio_pack_registry.h"

namespace {

constexpr float kDefaultFallbackGain = 0.22f;

}  // namespace

bool AudioPackApp::begin(const StoryAppContext& context) {
  context_ = context;
  snapshot_ = {};
  snapshot_.status = "READY";
  waitingAudioDone_ = false;
  emitAudioDone_ = false;
  return true;
}

void AudioPackApp::start(const StoryStepContext& stepContext) {
  snapshot_.bindingId = (stepContext.binding != nullptr) ? stepContext.binding->id : nullptr;
  snapshot_.active = true;
  snapshot_.status = "RUNNING";
  snapshot_.startedAtMs = stepContext.nowMs;
  waitingAudioDone_ = false;
  emitAudioDone_ = false;

  if (stepContext.step == nullptr || stepContext.step->resources.audioPackId == nullptr ||
      stepContext.step->resources.audioPackId[0] == '\0') {
    snapshot_.status = "NO_PACK";
    return;
  }

  const StoryAudioPackDef* pack = storyFindAudioPack(stepContext.step->resources.audioPackId);
  if (pack == nullptr) {
    snapshot_.status = "PACK_MISSING";
    emitAudioDone_ = true;
    return;
  }

  bool started = false;
  if (context_.startRandomTokenBase != nullptr && pack->token != nullptr && pack->token[0] != '\0') {
    started = context_.startRandomTokenBase(pack->token,
                                            "story_app_audio_pack",
                                            pack->allowSdFallback,
                                            pack->maxDurationMs);
  }

  if (!started && context_.startFallbackBaseFx != nullptr) {
    started = context_.startFallbackBaseFx(pack->fallbackEffect,
                                           pack->fallbackDurationMs,
                                           pack->gain > 0.0f ? pack->gain : kDefaultFallbackGain,
                                           "story_app_audio_fallback");
  }

  if (started) {
    waitingAudioDone_ = true;
    snapshot_.status = "AUDIO_PLAYING";
    return;
  }

  snapshot_.status = "AUDIO_FAILED";
  emitAudioDone_ = true;
}

void AudioPackApp::update(uint32_t nowMs, const StoryEventSink& sink) {
  if (!snapshot_.active) {
    return;
  }

  if (emitAudioDone_) {
    emitAudioDone_ = false;
    sink.emit(StoryEventType::kAudioDone, "AUDIO_DONE", 1, nowMs);
    snapshot_.status = "AUDIO_DONE";
    return;
  }

  if (!waitingAudioDone_) {
    return;
  }

  if (context_.audioService == nullptr) {
    waitingAudioDone_ = false;
    sink.emit(StoryEventType::kAudioDone, "AUDIO_DONE", 1, nowMs);
    snapshot_.status = "AUDIO_DONE";
    return;
  }

  if (!context_.audioService->isBaseBusy()) {
    waitingAudioDone_ = false;
    sink.emit(StoryEventType::kAudioDone, "AUDIO_DONE", 1, nowMs);
    snapshot_.status = "AUDIO_DONE";
  }
}

void AudioPackApp::stop(const char* reason) {
  snapshot_.active = false;
  snapshot_.status = (reason != nullptr && reason[0] != '\0') ? reason : "STOPPED";
  waitingAudioDone_ = false;
  emitAudioDone_ = false;
}

bool AudioPackApp::handleEvent(const StoryEvent& event, const StoryEventSink& sink) {
  (void)sink;
  if (!snapshot_.active) {
    return false;
  }
  if (event.type == StoryEventType::kSerial && strcmp(event.name, "STOP_AUDIO_PACK") == 0) {
    waitingAudioDone_ = false;
    emitAudioDone_ = true;
    snapshot_.status = "STOP_REQ";
    return true;
  }
  return false;
}

StoryAppSnapshot AudioPackApp::snapshot() const {
  return snapshot_;
}
