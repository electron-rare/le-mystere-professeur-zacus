#include "mp3_player.h"

#include <new>

#include <AudioFileSourceFS.h>
#include <AudioGeneratorMP3.h>
#include <AudioOutputI2S.h>
#include <FS.h>
#include <SD_MMC.h>

Mp3Player::Mp3Player(uint8_t i2sBclk,
                     uint8_t i2sLrc,
                     uint8_t i2sDout,
                     const char* mp3Path,
                     int8_t paEnablePin)
    : i2sBclk_(i2sBclk),
      i2sLrc_(i2sLrc),
      i2sDout_(i2sDout),
      paEnablePin_(paEnablePin),
      mp3Path_(mp3Path) {}

Mp3Player::~Mp3Player() {
  stop();
}

void Mp3Player::begin() {
  if (paEnablePin_ >= 0) {
    pinMode(paEnablePin_, OUTPUT);
    digitalWrite(paEnablePin_, HIGH);
  }
}

void Mp3Player::update(uint32_t nowMs) {
  refreshStorage(nowMs);
  if (!sdReady_ || trackCount_ == 0) {
    stop();
    return;
  }

  if (mp3_ == nullptr) {
    if (paused_) {
      return;
    }

    if (nowMs < nextRetryMs_) {
      return;
    }

    startCurrentTrack();
    return;
  }

  if (paused_) {
    return;
  }

  if (mp3_->isRunning()) {
    mp3_->loop();
    return;
  }

  stop();
  if (repeatMode_ == RepeatMode::kAll && trackCount_ > 0) {
    currentTrack_ = static_cast<uint16_t>((currentTrack_ + 1U) % trackCount_);
  }
  startCurrentTrack();
}

void Mp3Player::togglePause() {
  if (!sdReady_ || trackCount_ == 0) {
    return;
  }
  paused_ = !paused_;
}

void Mp3Player::restartTrack() {
  if (!sdReady_ || trackCount_ == 0) {
    return;
  }
  paused_ = false;
  stop();
  startCurrentTrack();
}

void Mp3Player::nextTrack() {
  if (!sdReady_ || trackCount_ == 0) {
    return;
  }

  paused_ = false;
  stop();
  currentTrack_ = static_cast<uint16_t>((currentTrack_ + 1U) % trackCount_);
  startCurrentTrack();
}

void Mp3Player::previousTrack() {
  if (!sdReady_ || trackCount_ == 0) {
    return;
  }

  paused_ = false;
  stop();
  if (currentTrack_ == 0) {
    currentTrack_ = static_cast<uint16_t>(trackCount_ - 1U);
  } else {
    --currentTrack_;
  }
  startCurrentTrack();
}

void Mp3Player::cycleRepeatMode() {
  repeatMode_ = (repeatMode_ == RepeatMode::kAll) ? RepeatMode::kOne : RepeatMode::kAll;
}

void Mp3Player::requestStorageRefresh() {
  forceRescan_ = true;
  nextMountAttemptMs_ = 0;
  nextRescanMs_ = 0;
}

void Mp3Player::setGain(float gain) {
  if (gain < 0.0f) {
    gain = 0.0f;
  } else if (gain > 1.0f) {
    gain = 1.0f;
  }

  gain_ = gain;
  if (i2sOut_ != nullptr) {
    i2sOut_->SetGain(gain_);
  }
}

float Mp3Player::gain() const {
  return gain_;
}

uint8_t Mp3Player::volumePercent() const {
  return static_cast<uint8_t>(gain_ * 100.0f);
}

bool Mp3Player::isPaused() const {
  return paused_;
}

bool Mp3Player::isSdReady() const {
  return sdReady_;
}

bool Mp3Player::hasTracks() const {
  return trackCount_ > 0;
}

bool Mp3Player::isPlaying() const {
  return mp3_ != nullptr && mp3_->isRunning() && !paused_;
}

uint16_t Mp3Player::trackCount() const {
  return trackCount_;
}

uint16_t Mp3Player::currentTrackNumber() const {
  if (trackCount_ == 0) {
    return 0;
  }
  return static_cast<uint16_t>(currentTrack_ + 1U);
}

String Mp3Player::currentTrackName() const {
  if (trackCount_ == 0) {
    return String();
  }
  return tracks_[currentTrack_];
}

RepeatMode Mp3Player::repeatMode() const {
  return repeatMode_;
}

const char* Mp3Player::repeatModeLabel() const {
  return (repeatMode_ == RepeatMode::kAll) ? "ALL" : "ONE";
}

bool Mp3Player::mountStorage(uint32_t nowMs) {
  if (!SD_MMC.begin("/sdcard", true)) {
    nextMountAttemptMs_ = nowMs + 2000;
    return false;
  }

  sdReady_ = true;
  nextCardCheckMs_ = nowMs + 1000;
  nextRescanMs_ = nowMs;
  Serial.println("[MP3] SD_MMC mounted.");
  scanTracks();
  return true;
}

void Mp3Player::unmountStorage(uint32_t nowMs) {
  stop();
  SD_MMC.end();

  sdReady_ = false;
  paused_ = false;
  trackCount_ = 0;
  currentTrack_ = 0;
  nextMountAttemptMs_ = nowMs + 1500;
  nextCardCheckMs_ = 0;
  nextRescanMs_ = 0;
  nextRetryMs_ = 0;

  Serial.println("[MP3] SD removed/unmounted.");
}

void Mp3Player::refreshStorage(uint32_t nowMs) {
  if (!sdReady_) {
    if (nowMs >= nextMountAttemptMs_) {
      mountStorage(nowMs);
    }
    return;
  }

  if (nowMs >= nextCardCheckMs_) {
    nextCardCheckMs_ = nowMs + 1000;
    if (SD_MMC.cardType() == CARD_NONE) {
      unmountStorage(nowMs);
      return;
    }
  }

  if (trackCount_ == 0 && nowMs >= nextRescanMs_) {
    scanTracks();
    forceRescan_ = false;
    nextRescanMs_ = nowMs + 3000;
    return;
  }

  if (forceRescan_) {
    scanTracks();
    forceRescan_ = false;
    nextRescanMs_ = nowMs + 3000;
    return;
  }

  if (currentTrack_ >= trackCount_) {
    currentTrack_ = 0;
  }
}

void Mp3Player::scanTracks() {
  trackCount_ = 0;

  fs::File root = SD_MMC.open("/");
  if (!root || !root.isDirectory()) {
    Serial.println("[MP3] Cannot open SD root.");
    return;
  }

  fs::File file = root.openNextFile();
  while (file) {
    if (!file.isDirectory()) {
      String filename = String(file.name());
      if (isMp3File(filename) && trackCount_ < kMaxTracks) {
        if (!filename.startsWith("/")) {
          filename = "/" + filename;
        }
        tracks_[trackCount_++] = filename;
      }
    }
    file.close();
    file = root.openNextFile();
  }
  root.close();

  sortTracks();

  if (trackCount_ == 0) {
    if (isMp3File(String(mp3Path_)) && SD_MMC.exists(mp3Path_)) {
      tracks_[0] = String(mp3Path_);
      trackCount_ = 1;
    } else {
      Serial.println("[MP3] No .mp3 file found on SD.");
      return;
    }
  }

  if (currentTrack_ >= trackCount_) {
    currentTrack_ = 0;
  }

  Serial.printf("[MP3] %u track(s) loaded.\n", static_cast<unsigned int>(trackCount_));
}

bool Mp3Player::isMp3File(const String& filename) {
  String lower = filename;
  lower.toLowerCase();
  return lower.endsWith(".mp3");
}

void Mp3Player::sortTracks() {
  if (trackCount_ < 2) {
    return;
  }

  for (uint16_t i = 0; i < trackCount_ - 1U; ++i) {
    for (uint16_t j = i + 1U; j < trackCount_; ++j) {
      if (tracks_[j].compareTo(tracks_[i]) < 0) {
        const String tmp = tracks_[i];
        tracks_[i] = tracks_[j];
        tracks_[j] = tmp;
      }
    }
  }
}

void Mp3Player::startCurrentTrack() {
  if (!sdReady_ || trackCount_ == 0 || currentTrack_ >= trackCount_) {
    return;
  }

  const String& trackPath = tracks_[currentTrack_];
  if (!SD_MMC.exists(trackPath.c_str())) {
    Serial.printf("[MP3] Missing track: %s\n", trackPath.c_str());
    scanTracks();
    nextRetryMs_ = millis() + 1000;
    return;
  }

  mp3File_ = new (std::nothrow) AudioFileSourceFS(SD_MMC, trackPath.c_str());
  i2sOut_ = new (std::nothrow) AudioOutputI2S();
  mp3_ = new (std::nothrow) AudioGeneratorMP3();
  if (mp3File_ == nullptr || i2sOut_ == nullptr || mp3_ == nullptr) {
    Serial.println("[MP3] Memory allocation failed.");
    stop();
    nextRetryMs_ = millis() + 1000;
    return;
  }

  i2sOut_->SetPinout(i2sBclk_, i2sLrc_, i2sDout_);
  i2sOut_->SetGain(gain_);
  if (!mp3_->begin(mp3File_, i2sOut_)) {
    Serial.println("[MP3] Unable to start playback.");
    stop();
    nextRetryMs_ = millis() + 1000;
    return;
  }

  Serial.printf("[MP3] Playing %u/%u: %s\n",
                static_cast<unsigned int>(currentTrack_ + 1U),
                static_cast<unsigned int>(trackCount_),
                trackPath.c_str());
}

void Mp3Player::stop() {
  if (mp3_ != nullptr) {
    if (mp3_->isRunning()) {
      mp3_->stop();
    }
    delete mp3_;
    mp3_ = nullptr;
  }

  if (mp3File_ != nullptr) {
    delete mp3File_;
    mp3File_ = nullptr;
  }

  if (i2sOut_ != nullptr) {
    delete i2sOut_;
    i2sOut_ = nullptr;
  }
}
