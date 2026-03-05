#include "audio_service.h"

#include <cstdio>
#include <cstring>

namespace {

AudioEffectId toEffect(FmRadioScanFx::Effect effect) {
  return static_cast<AudioEffectId>(effect);
}

FmRadioScanFx::Effect toFmEffect(AudioEffectId effect) {
  return static_cast<FmRadioScanFx::Effect>(effect);
}

}  // namespace

AudioService::AudioService(AsyncAudioService& baseAsync, FmRadioScanFx& baseFx, Mp3Player& mp3)
    : baseAsync_(baseAsync), baseFx_(baseFx), mp3_(mp3) {}

void AudioService::copyTag(char* out, size_t outLen, const char* tag) {
  if (out == nullptr || outLen == 0U) {
    return;
  }
  out[0] = '\0';
  if (tag == nullptr || tag[0] == '\0') {
    return;
  }
  snprintf(out, outLen, "%s", tag);
}

AudioService::Result AudioService::mapBaseResult(AsyncAudioService::Result result) {
  switch (result) {
    case AsyncAudioService::Result::kDone:
      return Result::kDone;
    case AsyncAudioService::Result::kTimeout:
      return Result::kTimeout;
    case AsyncAudioService::Result::kFailed:
      return Result::kFailed;
    case AsyncAudioService::Result::kCanceled:
      return Result::kCanceled;
    case AsyncAudioService::Result::kNone:
    default:
      return Result::kNone;
  }
}

bool AudioService::startBaseFs(fs::FS& storage,
                               const char* path,
                               float gain,
                               uint32_t timeoutMs,
                               const char* tag) {
  if (!baseAsync_.startFs(storage, path, gain, timeoutMs, tag)) {
    base_.lastResult = Result::kFailed;
    return false;
  }

  base_.active = true;
  base_.fsSource = true;
  base_.lastResult = Result::kStarted;
  base_.remainingMs = timeoutMs;
  base_.effect = AudioEffectId::kFmSweep;
  copyTag(base_.tag, sizeof(base_.tag), tag);
  return true;
}

bool AudioService::startBaseFx(AudioEffectId effect,
                               float gain,
                               uint32_t durationMs,
                               const char* tag) {
  if (!baseAsync_.startFx(baseFx_, toFmEffect(effect), durationMs, gain, tag)) {
    base_.lastResult = Result::kFailed;
    return false;
  }

  base_.active = true;
  base_.fsSource = false;
  base_.effect = effect;
  base_.lastResult = Result::kStarted;
  base_.remainingMs = durationMs;
  copyTag(base_.tag, sizeof(base_.tag), tag);
  return true;
}

bool AudioService::startOverlayFx(AudioEffectId effect,
                                  float gain,
                                  uint32_t durationMs,
                                  const char* tag) {
  if (!mp3_.isPlaying()) {
    overlay_.lastResult = Result::kFailed;
    return false;
  }

  mp3_.setFxOverlayGain(gain);
  if (!mp3_.triggerFx(effect, durationMs)) {
    overlay_.lastResult = Result::kFailed;
    return false;
  }

  overlay_.active = true;
  overlay_.fsSource = false;
  overlay_.effect = effect;
  overlay_.remainingMs = durationMs;
  overlay_.lastResult = Result::kStarted;
  overlayDeadlineMs_ = millis() + durationMs;
  copyTag(overlay_.tag, sizeof(overlay_.tag), tag);
  return true;
}

void AudioService::stopBase(const char* reason) {
  if (!baseAsync_.isBusy()) {
    return;
  }
  baseAsync_.cancel(reason);
  base_.active = false;
  base_.remainingMs = 0U;
  base_.lastResult = Result::kCanceled;
}

void AudioService::stopOverlay(const char* reason) {
  (void)reason;
  if (!mp3_.isFxActive() && !overlay_.active) {
    return;
  }
  mp3_.stopFx();
  overlay_.active = false;
  overlay_.remainingMs = 0U;
  overlay_.lastResult = Result::kCanceled;
  overlayDeadlineMs_ = 0U;
}

void AudioService::stopAll(const char* reason) {
  stopOverlay(reason);
  stopBase(reason);
}

void AudioService::update(uint32_t nowMs) {
  baseAsync_.update(nowMs);
  if (baseAsync_.hasEvent()) {
    const AsyncAudioService::Event event = baseAsync_.popEvent();
    base_.active = false;
    base_.remainingMs = 0U;
    base_.lastResult = mapBaseResult(event.result);
    if (event.kind == AsyncAudioService::Kind::kFx) {
      base_.fsSource = false;
      base_.effect = toEffect(baseFx_.effect());
    }
    if (event.tag[0] != '\0') {
      copyTag(base_.tag, sizeof(base_.tag), event.tag);
    }
  }

  if (base_.active && base_.remainingMs > 0U) {
    base_.remainingMs = base_.remainingMs - ((base_.remainingMs > 8U) ? 8U : base_.remainingMs);
  }

  const bool fxActive = mp3_.isFxActive();
  if (!fxActive && overlay_.active) {
    overlay_.active = false;
    overlay_.remainingMs = 0U;
    overlay_.lastResult = Result::kDone;
    overlayDeadlineMs_ = 0U;
  } else if (fxActive) {
    overlay_.active = true;
    overlay_.remainingMs = mp3_.fxRemainingMs();
    if (overlayDeadlineMs_ != 0U && static_cast<int32_t>(nowMs - overlayDeadlineMs_) >= 0) {
      overlay_.active = false;
      overlay_.remainingMs = 0U;
      overlay_.lastResult = Result::kDone;
      overlayDeadlineMs_ = 0U;
    }
  }
}

AudioService::AudioSnapshot AudioService::snapshot() const {
  AudioSnapshot out;
  out.base = base_;
  out.overlay = overlay_;
  return out;
}

bool AudioService::isBaseBusy() const {
  return baseAsync_.isBusy();
}

bool AudioService::isOverlayBusy() const {
  return overlay_.active;
}
