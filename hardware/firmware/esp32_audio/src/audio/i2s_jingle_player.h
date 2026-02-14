#pragma once

#include <Arduino.h>

class AudioFileSourcePROGMEM;
class AudioGeneratorRTTTL;
class AudioOutputI2S;

class I2sJinglePlayer {
 public:
  I2sJinglePlayer(uint8_t bclkPin, uint8_t wsPin, uint8_t doutPin, uint8_t i2sPort);
  ~I2sJinglePlayer();

  bool start(const char* rtttlSong, float gain);
  void update();
  void stop();
  bool isActive() const;

 private:
  void clearResources();

  uint8_t bclkPin_;
  uint8_t wsPin_;
  uint8_t doutPin_;
  uint8_t i2sPort_;
  AudioFileSourcePROGMEM* source_ = nullptr;
  AudioGeneratorRTTTL* generator_ = nullptr;
  AudioOutputI2S* output_ = nullptr;
  bool active_ = false;
};
