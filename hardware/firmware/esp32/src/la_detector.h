#pragma once

#include <Arduino.h>

#include "config.h"

class LaDetector {
 public:
  explicit LaDetector(uint8_t micPin);

  void begin();
  void update(uint32_t nowMs);
  bool isDetected() const;
  int8_t tuningOffset() const;
  uint8_t tuningConfidence() const;
  float targetRatio() const;
  float micMean() const;
  float micRms() const;
  uint16_t micMin() const;
  uint16_t micMax() const;
  uint16_t micPeakToPeak() const;

 private:
  float goertzelPower(const int16_t* x, uint16_t n, float fs, float targetHz) const;
  bool detect(const int16_t* samples,
              float* targetRatio,
              int8_t* tuningOffset,
              uint8_t* tuningConfidence,
              float* micMean,
              float* micRms,
              uint16_t* micMin,
              uint16_t* micMax) const;

  uint8_t micPin_;
  int16_t samples_[config::kDetectN] = {};
  uint16_t sampleIndex_ = 0;
  bool captureInProgress_ = false;
  uint32_t nextSampleUs_ = 0;
  uint32_t lastDetectMs_ = 0;
  bool detected_ = false;
  float targetRatio_ = 0.0f;
  int8_t tuningOffset_ = 0;
  uint8_t tuningConfidence_ = 0;
  float micMean_ = 0.0f;
  float micRms_ = 0.0f;
  uint16_t micMin_ = 0;
  uint16_t micMax_ = 0;
};
