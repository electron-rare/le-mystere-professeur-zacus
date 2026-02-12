#pragma once

#include <Arduino.h>

class KeypadAnalog {
 public:
  struct Thresholds {
    uint16_t releaseThreshold = 0;
    uint16_t keyMax[6] = {0, 0, 0, 0, 0, 0};
  };

  explicit KeypadAnalog(uint8_t adcPin);

  void begin();
  void update(uint32_t nowMs);

  bool consumePress(uint8_t* key, uint16_t* raw = nullptr);
  uint8_t currentKey() const;
  uint16_t lastRaw() const;
  const Thresholds& thresholds() const;
  void resetThresholdsToDefault();
  bool setKeyMax(uint8_t key, uint16_t rawMax);
  bool setReleaseThreshold(uint16_t rawMax);
  bool setThresholds(const Thresholds& thresholds);

 private:
  uint8_t decodeKey(uint16_t raw) const;
  static Thresholds defaultThresholds();
  static bool isThresholdsValid(const Thresholds& thresholds);

  uint8_t adcPin_;
  Thresholds thresholds_;
  uint32_t lastSampleMs_ = 0;
  uint32_t candidateSinceMs_ = 0;
  uint16_t lastRaw_ = 0;
  uint8_t candidateKey_ = 0;
  uint8_t stableKey_ = 0;
  bool pressPending_ = false;
  uint8_t pressKey_ = 0;
  uint16_t pressRaw_ = 0;
};
