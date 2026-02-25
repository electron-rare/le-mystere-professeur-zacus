#pragma once

#include <Arduino.h>

#if defined(USE_AUDIO) && (USE_AUDIO != 0)

#include <FS.h>

#include <vector>

namespace ui {
namespace audio {

class AudioPlayerService {
 public:
  enum class State : uint8_t {
    kStopped = 0,
    kPlaying,
    kPaused,
    kError,
  };

  struct Config {
    int8_t bclk = -1;
    int8_t ws = -1;
    int8_t dout = -1;
    uint8_t volume = 10;
    uint8_t max_volume = 21;
    bool use_task = false;
    uint8_t task_core = 0;
    uint16_t task_stack = 4096;
    uint8_t task_prio = 2;
    uint16_t stats_period_ms = 80;
  };

  struct Stats {
    State state = State::kStopped;
    uint32_t duration_s = 0;
    uint32_t position_s = 0;
    uint16_t vu = 0;
    uint32_t bitrate = 0;
    uint32_t samplerate = 0;
    uint8_t channels = 0;
    uint8_t bits_per_sample = 0;
    bool eof = false;
  };

  AudioPlayerService();
  ~AudioPlayerService();

  AudioPlayerService(const AudioPlayerService&) = delete;
  AudioPlayerService& operator=(const AudioPlayerService&) = delete;

  bool begin(fs::FS* fs, const char* base_dir, const Config& cfg);
  void end();

  size_t scanPlaylist();

  void playIndex(size_t index);
  void playPath(const char* path);
  void togglePause();
  void stop();
  void next();
  void prev();
  void seek(uint32_t position_s);

  void setVolume(uint8_t volume);
  uint8_t volume() const;

  size_t trackCount() const;
  size_t currentIndex() const;
  const char* currentPath() const;
  const char* trackPath(size_t index) const;

  Stats stats() const;

  bool taskMode() const;
  void loopOnce();

 private:
  static bool isAudioFileName(const char* name);
  void refreshStats(uint32_t now_ms);
  bool startPath(const char* path);

  struct Impl;
  Impl* impl_ = nullptr;

  Config cfg_;
  fs::FS* fs_ = nullptr;
  String base_dir_;
  std::vector<String> tracks_;
  size_t current_index_ = 0U;
  Stats stats_;
  bool ready_ = false;
  bool paused_ = false;
  bool was_running_ = false;
  uint32_t last_stats_ms_ = 0U;
};

}  // namespace audio
}  // namespace ui

#endif  // USE_AUDIO
