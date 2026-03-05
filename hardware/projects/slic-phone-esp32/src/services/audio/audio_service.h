#pragma once

#include <Arduino.h>
#include <FS.h>

#include "../../audio/effects/audio_effect_id.h"
#include "../../audio/fm_radio_scan_fx.h"
#include "../../audio/mp3_player.h"
#include "async_audio_service.h"

class AudioService {
 public:
  enum class Channel : uint8_t {
    kBase = 0,
    kOverlay,
  };

  enum class Result : uint8_t {
    kNone = 0,
    kStarted,
    kDone,
    kTimeout,
    kFailed,
    kCanceled,
  };

  struct ChannelSnapshot {
    bool active = false;
    bool fsSource = false;
    AudioEffectId effect = AudioEffectId::kFmSweep;
    uint32_t remainingMs = 0;
    Result lastResult = Result::kNone;
    char tag[24] = {};
  };

  struct AudioSnapshot {
    ChannelSnapshot base;
    ChannelSnapshot overlay;
  };

  AudioService(AsyncAudioService& baseAsync, FmRadioScanFx& baseFx, Mp3Player& mp3);

  bool startBaseFs(fs::FS& storage,
                   const char* path,
                   float gain,
                   uint32_t timeoutMs,
                   const char* tag);
  bool startBaseFx(AudioEffectId effect,
                   float gain,
                   uint32_t durationMs,
                   const char* tag);
  bool startOverlayFx(AudioEffectId effect,
                      float gain,
                      uint32_t durationMs,
                      const char* tag);

  void stopBase(const char* reason);
  void stopOverlay(const char* reason);
  void stopAll(const char* reason);

  void update(uint32_t nowMs);
  AudioSnapshot snapshot() const;

  bool isBaseBusy() const;
  bool isOverlayBusy() const;

 private:
  static Result mapBaseResult(AsyncAudioService::Result result);
  static void copyTag(char* out, size_t outLen, const char* tag);

  AsyncAudioService& baseAsync_;
  FmRadioScanFx& baseFx_;
  Mp3Player& mp3_;

  ChannelSnapshot base_;
  ChannelSnapshot overlay_;
  uint32_t overlayDeadlineMs_ = 0U;
};
