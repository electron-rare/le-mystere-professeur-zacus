// audio_manager.h - audio playback over I2S for Freenove all-in-one firmware.
#pragma once

#include <Arduino.h>

class AudioManager {
 public:
  using AudioDoneCallback = void (*)(const char* track, void* ctx);

  AudioManager();
  ~AudioManager();

  bool begin();
  bool play(const char* filename);
  bool playDiagnosticTone();
  void stop();
  void update();
  bool isPlaying() const;

  void setVolume(uint8_t volume);
  uint8_t volume() const;
  const char* currentTrack() const;
  bool setOutputProfile(uint8_t profile_index);
  uint8_t outputProfile() const;
  uint8_t outputProfileCount() const;
  const char* outputProfileLabel(uint8_t profile_index) const;

  void setAudioDoneCallback(AudioDoneCallback cb, void* ctx);

 private:
  struct AudioChannel {
    class AudioGenerator* decoder = nullptr;
    class AudioFileSource* source = nullptr;
    class AudioOutputMixerStub* stub = nullptr;
    String track;
  };

  bool ensureOutput();
  void applyOutputProfile();
  bool playOnChannel(uint8_t channel_index, class AudioFileSource* source, class AudioGenerator* decoder, const char* track);
  void stopChannel(uint8_t channel_index);
  bool anyChannelRunning() const;
  uint8_t selectTargetChannel() const;
  void applyChannelGains();
  void startCrossfade(uint8_t from_channel, uint8_t to_channel);
  void stopCrossfade();
  void stopAllChannels();
  void finishPlaybackAndNotify(const char* track);

  class AudioOutputI2S* output_ = nullptr;
  class AudioOutputMixer* mixer_ = nullptr;
  AudioChannel channels_[2];
  uint8_t active_channel_ = 0U;
  bool crossfade_active_ = false;
  uint8_t crossfade_from_ = 0U;
  uint8_t crossfade_to_ = 0U;
  uint32_t crossfade_started_ms_ = 0U;
  uint16_t crossfade_duration_ms_ = 750U;

  bool playing_ = false;
  bool using_diagnostic_tone_ = false;
  uint8_t volume_ = 21;
  uint8_t output_profile_ = 0U;
  String current_track_;
  AudioDoneCallback done_cb_ = nullptr;
  void* done_ctx_ = nullptr;
};
