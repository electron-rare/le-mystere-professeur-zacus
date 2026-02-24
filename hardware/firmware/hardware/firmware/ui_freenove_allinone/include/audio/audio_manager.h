// audio_manager.h - Freenove audio playback manager backed by ESP32-audioI2S.
#pragma once

#include <Arduino.h>

#include "ui_freenove_config.h"

class Audio;

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
  bool setFxProfile(uint8_t fx_profile_index);
  uint8_t fxProfile() const;
  uint8_t fxProfileCount() const;
  const char* fxProfileLabel(uint8_t fx_profile_index) const;
  const char* activeCodec() const;
  uint16_t activeBitrateKbps() const;

  void setAudioDoneCallback(AudioDoneCallback cb, void* ctx);

 private:
  enum class AudioCodec : uint8_t {
    kUnknown = 0U,
    kMp3 = 1U,
    kWav = 2U,
    kAac = 3U,
    kFlac = 4U,
  };

  bool ensurePlayer();
  bool requestPlay(const char* filename, bool diagnostic_tone);
  void applyOutputProfile();
  void applyFxProfile();
  bool normalizeTrackPath(const char* input, String& out_path, bool& out_use_sd) const;
  bool trackExists(const String& path, bool use_sd) const;
  bool detectTrackCodecAndBitrate(const String& path, bool use_sd, AudioCodec& codec, uint16_t& bitrate_kbps) const;
  const char* codecLabel(AudioCodec codec) const;
  bool openTrack(const String& path, bool use_sd);
  bool beginTrackPlayback(const String& path,
                          bool use_sd,
                          AudioCodec codec,
                          uint16_t bitrate_kbps,
                          bool diagnostic_tone);
  void scheduleTrackStart(const String& path,
                          bool use_sd,
                          AudioCodec codec,
                          uint16_t bitrate_kbps,
                          bool diagnostic_tone,
                          uint32_t earliest_ms);
  void tryStartPendingTrack(uint32_t now_ms);
  void createRtosState();
  void destroyRtosState();
  bool startAudioPump();
  void stopAudioPump();
  void audioPumpLoop();
  static void audioPumpTaskEntry(void* arg);
  void processPendingPlaybackEvents();
  void enqueuePlaybackDone(const char* track);
  bool takeStateLock(uint32_t timeout_ms) const;
  void releaseStateLock() const;
  void clearTrackState();
  void finishPlaybackAndNotify();

  Audio* player_ = nullptr;
  struct AudioRtosState;
  AudioRtosState* rtos_state_ = nullptr;
  bool pump_task_enabled_ = false;
  bool begun_ = false;
  bool playing_ = false;
  bool using_diagnostic_tone_ = false;
  uint8_t volume_ = FREENOVE_AUDIO_MAX_VOLUME;
  uint8_t fx_profile_ = 0U;
  uint8_t output_profile_ = 0U;
  String current_track_;
  AudioCodec active_codec_ = AudioCodec::kUnknown;
  uint16_t active_bitrate_kbps_ = 0U;
  bool active_use_sd_ = false;
  bool pending_start_ = false;
  String pending_track_;
  AudioCodec pending_codec_ = AudioCodec::kUnknown;
  uint16_t pending_bitrate_kbps_ = 0U;
  bool pending_use_sd_ = false;
  bool pending_diagnostic_tone_ = false;
  uint32_t reopen_earliest_ms_ = 0U;
  mutable char current_track_snapshot_[96] = {0};
  AudioDoneCallback done_cb_ = nullptr;
  void* done_ctx_ = nullptr;
};
