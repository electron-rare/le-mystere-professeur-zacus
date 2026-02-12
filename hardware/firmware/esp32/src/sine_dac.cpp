#include "sine_dac.h"

SineDac::SineDac(uint8_t pin, float freqHz, uint16_t sampleRate)
    : pin_(pin),
      freqHz_(freqHz),
      sampleRate_(sampleRate),
      periodUs_(sampleRate > 0 ? (1000000UL / sampleRate) : 1000UL) {}

void SineDac::begin() {
  if (pin_ == 0xFF) {
    available_ = false;
    enabled_ = false;
    return;
  }
  available_ = isDacCapablePin(pin_);
  if (!available_) {
    enabled_ = false;
    Serial.printf("[SINE] GPIO%u n'est pas DAC (DAC reels: GPIO25/26). Sine analogique desactive.\n",
                  static_cast<unsigned int>(pin_));
    return;
  }
  buildTable();
}

void SineDac::update() {
  if (!available_ || !enabled_ || pin_ == 0xFF) {
    return;
  }

  const uint32_t nowUs = micros();
  if ((nowUs - lastMicros_) < periodUs_) {
    return;
  }

  lastMicros_ = nowUs;
  const float step = (freqHz_ * static_cast<float>(kTableSize)) / static_cast<float>(sampleRate_);
  phaseAcc_ += step;
  if (phaseAcc_ >= kTableSize) {
    phaseAcc_ -= kTableSize;
  }

  const uint8_t sample = table_[static_cast<uint16_t>(phaseAcc_)];
  dacWrite(pin_, sample);
}

void SineDac::setEnabled(bool enabled) {
  if (!available_) {
    enabled_ = false;
    return;
  }
  enabled_ = enabled;
  if (!enabled_ && pin_ != 0xFF) {
    dacWrite(pin_, 128);
  }
}

void SineDac::setFrequency(float freqHz) {
  if (freqHz < 20.0f) {
    freqHz = 20.0f;
  } else if (freqHz > 2000.0f) {
    freqHz = 2000.0f;
  }
  freqHz_ = freqHz;
}

float SineDac::frequency() const {
  return freqHz_;
}

bool SineDac::isEnabled() const {
  return available_ && enabled_;
}

bool SineDac::isAvailable() const {
  return available_;
}

bool SineDac::isDacCapablePin(uint8_t pin) {
  return pin == 25U || pin == 26U;
}

void SineDac::buildTable() {
  for (uint16_t i = 0; i < kTableSize; ++i) {
    const float phase = 2.0f * PI * static_cast<float>(i) / static_cast<float>(kTableSize);
    const float normalized = 0.5f + 0.5f * sinf(phase);
    table_[i] = static_cast<uint8_t>(normalized * 255.0f);
  }
}
