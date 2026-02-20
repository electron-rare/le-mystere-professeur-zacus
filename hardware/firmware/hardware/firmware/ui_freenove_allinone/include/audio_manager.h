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
  bool ensureOutput();
  void applyOutputProfile();
  void stopDecoder();
  void finishPlaybackAndNotify();

  class AudioGenerator* decoder_ = nullptr;
  class AudioFileSource* source_ = nullptr;
  class AudioOutputI2S* output_ = nullptr;

  bool playing_ = false;
  bool using_diagnostic_tone_ = false;
  uint8_t volume_ = 21;
  uint8_t output_profile_ = 0U;
  String current_track_;
  AudioDoneCallback done_cb_ = nullptr;
  void* done_ctx_ = nullptr;
};
