#include "ui/audio_player/audio_player_service.h"

#if defined(USE_AUDIO) && (USE_AUDIO != 0)

#include <Audio.h>

#include <algorithm>
#include <cstring>

#include "ui_freenove_config.h"

namespace ui {
namespace audio {

struct AudioPlayerService::Impl {
  Audio audio;
};

static uint8_t clampVolume(uint8_t value, uint8_t max_value) {
  if (value > max_value) {
    return max_value;
  }
  return value;
}

AudioPlayerService::AudioPlayerService() = default;

AudioPlayerService::~AudioPlayerService() {
  end();
}

bool AudioPlayerService::begin(fs::FS* fs, const char* base_dir, const Config& cfg) {
  end();
  if (fs == nullptr) {
    return false;
  }

  fs_ = fs;
  cfg_ = cfg;
  base_dir_ = (base_dir != nullptr && base_dir[0] != '\0') ? String(base_dir) : String("/");

  if (cfg_.bclk < 0) {
    cfg_.bclk = FREENOVE_I2S_BCK;
  }
  if (cfg_.ws < 0) {
    cfg_.ws = FREENOVE_I2S_WS;
  }
  if (cfg_.dout < 0) {
    cfg_.dout = FREENOVE_I2S_DOUT;
  }
  if (cfg_.max_volume == 0U) {
    cfg_.max_volume = static_cast<uint8_t>(FREENOVE_AUDIO_MAX_VOLUME);
  }

  impl_ = new Impl();
  impl_->audio.setPinout(cfg_.bclk, cfg_.ws, cfg_.dout);
  impl_->audio.setVolume(clampVolume(cfg_.volume, cfg_.max_volume));

  stats_ = {};
  stats_.state = State::kStopped;
  ready_ = true;
  paused_ = false;
  was_running_ = false;
  last_stats_ms_ = 0U;
  return true;
}

void AudioPlayerService::end() {
  ready_ = false;
  paused_ = false;
  was_running_ = false;
  last_stats_ms_ = 0U;

  if (impl_ != nullptr) {
    impl_->audio.stopSong();
    delete impl_;
    impl_ = nullptr;
  }

  tracks_.clear();
  current_index_ = 0U;
  base_dir_.remove(0);
  fs_ = nullptr;
  stats_ = {};
  stats_.state = State::kStopped;
}

size_t AudioPlayerService::scanPlaylist() {
  tracks_.clear();
  current_index_ = 0U;

  if (!ready_ || fs_ == nullptr) {
    return 0U;
  }

  File dir = fs_->open(base_dir_);
  if (!dir || !dir.isDirectory()) {
    return 0U;
  }

  File entry = dir.openNextFile();
  while (entry) {
    if (!entry.isDirectory()) {
      const char* name = entry.name();
      if (name != nullptr && isAudioFileName(name)) {
        String path = String(name);
        if (!path.startsWith("/")) {
          path = base_dir_ + "/" + path;
        }
        tracks_.push_back(path);
      }
    }
    entry = dir.openNextFile();
  }

  std::sort(tracks_.begin(), tracks_.end(), [](const String& a, const String& b) {
    return a.compareTo(b) < 0;
  });

  return tracks_.size();
}

bool AudioPlayerService::startPath(const char* path) {
  if (!ready_ || impl_ == nullptr || fs_ == nullptr || path == nullptr || path[0] == '\0') {
    return false;
  }
  impl_->audio.stopSong();
  const bool ok = impl_->audio.connecttoFS(*fs_, path);
  stats_.state = ok ? State::kPlaying : State::kError;
  stats_.eof = false;
  paused_ = false;
  was_running_ = ok;
  return ok;
}

void AudioPlayerService::playIndex(size_t index) {
  if (tracks_.empty()) {
    stats_.state = State::kError;
    return;
  }
  if (index >= tracks_.size()) {
    index = 0U;
  }
  current_index_ = index;
  (void)startPath(tracks_[current_index_].c_str());
}

void AudioPlayerService::playPath(const char* path) {
  if (!startPath(path)) {
    stats_.state = State::kError;
  }
}

void AudioPlayerService::togglePause() {
  if (!ready_) {
    return;
  }
  if (stats_.state == State::kPlaying) {
    if (impl_ != nullptr) {
      impl_->audio.stopSong();
    }
    paused_ = true;
    stats_.state = State::kPaused;
    was_running_ = false;
    return;
  }
  if (stats_.state == State::kPaused) {
    playIndex(current_index_);
  }
}

void AudioPlayerService::stop() {
  if (!ready_) {
    return;
  }
  if (impl_ != nullptr) {
    impl_->audio.stopSong();
  }
  paused_ = false;
  was_running_ = false;
  stats_.state = State::kStopped;
  stats_.duration_s = 0U;
  stats_.position_s = 0U;
  stats_.eof = false;
}

void AudioPlayerService::next() {
  if (tracks_.empty()) {
    stop();
    return;
  }
  size_t next_index = current_index_ + 1U;
  if (next_index >= tracks_.size()) {
    next_index = 0U;
  }
  playIndex(next_index);
}

void AudioPlayerService::prev() {
  if (tracks_.empty()) {
    stop();
    return;
  }
  size_t prev_index = (current_index_ == 0U) ? (tracks_.size() - 1U) : (current_index_ - 1U);
  playIndex(prev_index);
}

void AudioPlayerService::seek(uint32_t position_s) {
  (void)position_s;
}

void AudioPlayerService::setVolume(uint8_t volume) {
  cfg_.volume = clampVolume(volume, cfg_.max_volume);
  if (impl_ != nullptr) {
    impl_->audio.setVolume(cfg_.volume);
  }
}

uint8_t AudioPlayerService::volume() const {
  return cfg_.volume;
}

size_t AudioPlayerService::trackCount() const {
  return tracks_.size();
}

size_t AudioPlayerService::currentIndex() const {
  return current_index_;
}

const char* AudioPlayerService::currentPath() const {
  if (tracks_.empty() || current_index_ >= tracks_.size()) {
    return "";
  }
  return tracks_[current_index_].c_str();
}

const char* AudioPlayerService::trackPath(size_t index) const {
  if (index >= tracks_.size()) {
    return "";
  }
  return tracks_[index].c_str();
}

AudioPlayerService::Stats AudioPlayerService::stats() const {
  return stats_;
}

bool AudioPlayerService::taskMode() const {
  return false;
}

void AudioPlayerService::refreshStats(uint32_t now_ms) {
  if (!ready_ || impl_ == nullptr) {
    return;
  }
  if (cfg_.stats_period_ms == 0U) {
    return;
  }
  if ((now_ms - last_stats_ms_) < cfg_.stats_period_ms) {
    return;
  }
  last_stats_ms_ = now_ms;

  stats_.duration_s = impl_->audio.getAudioFileDuration();
  stats_.position_s = impl_->audio.getAudioCurrentTime();
  stats_.bitrate = impl_->audio.getBitRate();
  stats_.samplerate = impl_->audio.getSampleRate();
  stats_.channels = static_cast<uint8_t>(impl_->audio.getChannels());
  stats_.bits_per_sample = static_cast<uint8_t>(impl_->audio.getBitsPerSample());
  stats_.vu = 0U;
}

void AudioPlayerService::loopOnce() {
  if (!ready_ || impl_ == nullptr) {
    return;
  }

  if (stats_.state == State::kPlaying && !paused_) {
    impl_->audio.loop();
  }

  const bool running = impl_->audio.isRunning();
  if (was_running_ && !running && stats_.state == State::kPlaying && !paused_) {
    stats_.eof = true;
    next();
  }
  was_running_ = running;

  if (!running && stats_.state == State::kPlaying && !paused_) {
    stats_.state = State::kStopped;
  }

  refreshStats(millis());
}

bool AudioPlayerService::isAudioFileName(const char* name) {
  if (name == nullptr) {
    return false;
  }
  const char* dot = std::strrchr(name, '.');
  if (dot == nullptr) {
    return false;
  }
  ++dot;
  if (*dot == '\0') {
    return false;
  }

  char ext[8] = {0};
  size_t i = 0U;
  while (dot[i] != '\0' && i < sizeof(ext) - 1U) {
    char c = dot[i];
    if (c >= 'A' && c <= 'Z') {
      c = static_cast<char>(c + ('a' - 'A'));
    }
    ext[i] = c;
    ++i;
  }

  return std::strcmp(ext, "mp3") == 0 ||
         std::strcmp(ext, "wav") == 0 ||
         std::strcmp(ext, "m4a") == 0 ||
         std::strcmp(ext, "aac") == 0 ||
         std::strcmp(ext, "flac") == 0 ||
         std::strcmp(ext, "opus") == 0 ||
         std::strcmp(ext, "ogg") == 0;
}

}  // namespace audio
}  // namespace ui

#endif  // USE_AUDIO
