#pragma once

#include <Arduino.h>

class SineDac {
 public:
  SineDac(uint8_t pin, float freqHz, uint16_t sampleRate);

  void begin();
  void update();
  void setEnabled(bool enabled);
  void setFrequency(float freqHz);
  float frequency() const;
  bool isEnabled() const;
  bool isAvailable() const;

 private:
  static constexpr uint16_t kTableSize = 128;
  static bool isDacCapablePin(uint8_t pin);

  void buildTable();

  uint8_t pin_;
  float freqHz_;
  uint16_t sampleRate_;
  uint8_t table_[kTableSize] = {};
  uint32_t lastMicros_ = 0;
  uint32_t periodUs_;
  float phaseAcc_ = 0.0f;
  bool enabled_ = false;
  bool available_ = false;
};
