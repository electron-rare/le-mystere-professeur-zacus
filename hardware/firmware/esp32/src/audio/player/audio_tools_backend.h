#pragma once

#include <Arduino.h>

class AudioToolsBackend {
 public:
  AudioToolsBackend(uint8_t i2sBclk, uint8_t i2sLrc, uint8_t i2sDout, uint8_t i2sPort);

  bool start(const char* path, float gain);
  void update();
  void stop();

  bool isActive() const;
  bool canHandlePath(const char* path) const;
  const char* lastError() const;

  void setGain(float gain);
  float gain() const;

 private:
  bool setupI2s();
  bool setupDecoderForPath(const char* path);
  void setLastError(const char* code);

  uint8_t i2sBclk_;
  uint8_t i2sLrc_;
  uint8_t i2sDout_;
  uint8_t i2sPort_;
  float gain_ = 0.20f;

  bool active_ = false;
  bool eof_ = false;
  uint8_t idleLoops_ = 0U;
  char lastError_[24] = "OK";

  void* i2s_ = nullptr;
  void* decoder_ = nullptr;
  void* encoded_ = nullptr;
  void* copier_ = nullptr;
  void* file_ = nullptr;
};
