// media_manager.h - lightweight media file browsing/playback/record controls.
#pragma once

#include <Arduino.h>

class AudioManager;

class MediaManager {
 public:
  MediaManager() = default;
  ~MediaManager() = default;
  MediaManager(const MediaManager&) = delete;
  MediaManager& operator=(const MediaManager&) = delete;
  MediaManager(MediaManager&&) = delete;
  MediaManager& operator=(MediaManager&&) = delete;

  struct Config {
    char music_dir[32] = "/music";
    char picture_dir[32] = "/picture";
    char record_dir[32] = "/recorder";
    uint16_t record_max_seconds = 30U;
    bool auto_stop_record_on_step_change = true;
  };

  struct Snapshot {
    bool ready = false;
    bool playing = false;
    bool recording = false;
    bool last_ok = true;
    bool record_simulated = true;
    uint16_t record_limit_seconds = 30U;
    uint16_t record_elapsed_seconds = 0U;
    uint32_t record_started_ms = 0U;
    char playing_path[128] = "";
    char record_file[128] = "";
    char last_error[64] = "";
    char music_dir[32] = "/music";
    char picture_dir[32] = "/picture";
    char record_dir[32] = "/recorder";
  };

  bool begin(const Config& config);
  void update(uint32_t now_ms, AudioManager* audio);
  void noteStepChange();
  bool listFiles(const char* kind, String* out_json) const;
  bool play(const char* path, AudioManager* audio);
  bool stop(AudioManager* audio);
  bool startRecording(uint16_t seconds, const char* filename_hint);
  bool stopRecording();
  Snapshot snapshot() const;

 private:
  void setLastError(const char* message);
  void clearLastError();
  String normalizeDir(const char* path) const;
  String resolveKindDir(const char* kind) const;
  String sanitizeFilename(const char* hint, const char* default_prefix, const char* extension) const;
  bool ensureDir(const char* path) const;
  bool writeEmptyWav(const char* path) const;

  Config config_;
  Snapshot snapshot_;
};
