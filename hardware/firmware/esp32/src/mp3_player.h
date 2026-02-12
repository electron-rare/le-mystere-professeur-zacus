#pragma once

#include <Arduino.h>

class AudioFileSourceFS;
class AudioGenerator;
class AudioOutputI2S;

enum class AudioCodec : uint8_t {
  kUnknown = 0,
  kMp3,
  kWav,
  kAac,
  kFlac,
  kOpus,
};

enum class RepeatMode : uint8_t {
  kAll = 0,
  kOne = 1,
};

class Mp3Player {
 public:
  Mp3Player(uint8_t i2sBclk,
            uint8_t i2sLrc,
            uint8_t i2sDout,
            const char* mp3Path,
            int8_t paEnablePin = -1);
  ~Mp3Player();

  void begin();
  void update(uint32_t nowMs, bool allowPlayback = true);
  void togglePause();
  void restartTrack();
  void nextTrack();
  void previousTrack();
  void cycleRepeatMode();
  void requestStorageRefresh();
  void setGain(float gain);
  float gain() const;
  uint8_t volumePercent() const;
  bool isPaused() const;
  bool isSdReady() const;
  bool hasTracks() const;
  bool isPlaying() const;
  uint16_t trackCount() const;
  uint16_t currentTrackNumber() const;
  String currentTrackName() const;
  RepeatMode repeatMode() const;
  const char* repeatModeLabel() const;

 private:
  static constexpr uint16_t kMaxTracks = 64;

  bool mountStorage(uint32_t nowMs);
  void unmountStorage(uint32_t nowMs);
  void refreshStorage(uint32_t nowMs);
  void scanTracks();
  static bool isSupportedAudioFile(const String& filename);
  static AudioCodec codecForPath(const String& filename);
  static const char* codecLabel(AudioCodec codec);
  void sortTracks();
  static AudioGenerator* createDecoder(AudioCodec codec);

  void startCurrentTrack();
  void stop();

  uint8_t i2sBclk_;
  uint8_t i2sLrc_;
  uint8_t i2sDout_;
  int8_t paEnablePin_;
  const char* mp3Path_;

  bool sdReady_ = false;
  bool paused_ = false;
  float gain_ = 0.20f;
  uint32_t nextMountAttemptMs_ = 0;
  uint32_t nextCardCheckMs_ = 0;
  uint32_t nextRescanMs_ = 0;
  uint32_t nextRetryMs_ = 0;
  uint16_t trackCount_ = 0;
  uint16_t currentTrack_ = 0;
  String tracks_[kMaxTracks];
  RepeatMode repeatMode_ = RepeatMode::kAll;
  bool forceRescan_ = false;
  AudioCodec activeCodec_ = AudioCodec::kUnknown;
  AudioGenerator* decoder_ = nullptr;
  AudioFileSourceFS* mp3File_ = nullptr;
  AudioOutputI2S* i2sOut_ = nullptr;
};
