#pragma once

#include <Arduino.h>

class AudioOutputI2S;

class FmRadioScanFx {
 public:
  FmRadioScanFx(uint8_t bclkPin, uint8_t wsPin, uint8_t doutPin, uint8_t i2sPort);
  ~FmRadioScanFx();

  void setGain(float gain);
  void setSampleRate(uint32_t sampleRateHz);

  bool start();
  void stop();
  void update(uint32_t nowMs, uint16_t chunkMs);
  bool isActive() const;

  bool playBlocking(uint32_t durationMs, uint16_t chunkMs = 22U);

 private:
  void resetSynthesisState();
  int16_t nextSample();

  uint8_t bclkPin_;
  uint8_t wsPin_;
  uint8_t doutPin_;
  uint8_t i2sPort_;

  AudioOutputI2S* output_ = nullptr;
  bool active_ = false;
  float gain_ = 0.18f;
  uint32_t sampleRateHz_ = 22050U;

  float sweepPhase_ = 0.0f;
  float beatPhase_ = 0.0f;
  float noiseLp_ = 0.0f;
  float crackle_ = 0.0f;
  float stationBlend_ = 0.0f;
  uint32_t sweepCycle_ = 0;
  uint32_t sweepPosInCycle_ = 0;
  uint32_t sampleClock_ = 0;
};
