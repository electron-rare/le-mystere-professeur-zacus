#include "i2s_jingle_player.h"

#include <cstring>
#include <new>

#include <AudioFileSourcePROGMEM.h>
#include <AudioGeneratorRTTTL.h>
#include <AudioOutputI2S.h>

I2sJinglePlayer::I2sJinglePlayer(uint8_t bclkPin, uint8_t wsPin, uint8_t doutPin, uint8_t i2sPort)
    : bclkPin_(bclkPin), wsPin_(wsPin), doutPin_(doutPin), i2sPort_(i2sPort) {}

I2sJinglePlayer::~I2sJinglePlayer() {
  stop();
}

bool I2sJinglePlayer::start(const char* rtttlSong, float gain) {
  if (rtttlSong == nullptr || rtttlSong[0] == '\0') {
    return false;
  }

  stop();

  const size_t songLen = strlen(rtttlSong);
  if (songLen == 0U) {
    return false;
  }

  source_ = new (std::nothrow) AudioFileSourcePROGMEM();
  output_ = new (std::nothrow)
      AudioOutputI2S(static_cast<int>(i2sPort_), AudioOutputI2S::EXTERNAL_I2S);
  generator_ = new (std::nothrow) AudioGeneratorRTTTL();

  if (source_ == nullptr || output_ == nullptr || generator_ == nullptr) {
    clearResources();
    return false;
  }

  if (!source_->open(rtttlSong, static_cast<uint32_t>(songLen))) {
    clearResources();
    return false;
  }

  output_->SetPinout(static_cast<int>(bclkPin_), static_cast<int>(wsPin_), static_cast<int>(doutPin_));
  output_->SetOutputModeMono(true);
  output_->SetGain(gain);
  generator_->SetRate(22050);

  if (!generator_->begin(source_, output_)) {
    clearResources();
    return false;
  }

  active_ = true;
  return true;
}

void I2sJinglePlayer::update() {
  if (!active_ || generator_ == nullptr) {
    return;
  }

  if (!generator_->loop() || !generator_->isRunning()) {
    stop();
  }
}

void I2sJinglePlayer::stop() {
  if (!active_ && source_ == nullptr && output_ == nullptr && generator_ == nullptr) {
    return;
  }

  active_ = false;
  clearResources();
}

bool I2sJinglePlayer::isActive() const {
  return active_;
}

void I2sJinglePlayer::clearResources() {
  if (generator_ != nullptr) {
    generator_->stop();
    delete generator_;
    generator_ = nullptr;
  }

  if (output_ != nullptr) {
    output_->stop();
    delete output_;
    output_ = nullptr;
  }

  if (source_ != nullptr) {
    source_->close();
    delete source_;
    source_ = nullptr;
  }
}
