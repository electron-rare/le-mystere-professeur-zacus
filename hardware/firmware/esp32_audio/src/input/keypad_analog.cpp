#include "keypad_analog.h"

#include "../config.h"

KeypadAnalog::KeypadAnalog(uint8_t adcPin) : adcPin_(adcPin), thresholds_(defaultThresholds()) {}

void KeypadAnalog::begin() {
  analogReadResolution(12);
  analogSetPinAttenuation(adcPin_, ADC_11db);
  resetThresholdsToDefault();
}

void KeypadAnalog::update(uint32_t nowMs) {
  if ((nowMs - lastSampleMs_) < config::kKeysSampleEveryMs) {
    return;
  }
  lastSampleMs_ = nowMs;

  lastRaw_ = static_cast<uint16_t>(analogRead(adcPin_));
  const uint8_t key = decodeKey(lastRaw_);

  if (key != candidateKey_) {
    candidateKey_ = key;
    candidateSinceMs_ = nowMs;
    return;
  }

  if (key == stableKey_) {
    return;
  }

  if ((nowMs - candidateSinceMs_) < config::kKeysDebounceMs) {
    return;
  }

  stableKey_ = key;
  if (stableKey_ == 0) {
    return;
  }

  pressPending_ = true;
  pressKey_ = stableKey_;
  pressRaw_ = lastRaw_;
}

bool KeypadAnalog::consumePress(uint8_t* key, uint16_t* raw) {
  if (!pressPending_) {
    return false;
  }

  pressPending_ = false;
  if (key != nullptr) {
    *key = pressKey_;
  }
  if (raw != nullptr) {
    *raw = pressRaw_;
  }
  return true;
}

uint8_t KeypadAnalog::currentKey() const {
  return stableKey_;
}

uint16_t KeypadAnalog::lastRaw() const {
  return lastRaw_;
}

const KeypadAnalog::Thresholds& KeypadAnalog::thresholds() const {
  return thresholds_;
}

void KeypadAnalog::resetThresholdsToDefault() {
  thresholds_ = defaultThresholds();
}

bool KeypadAnalog::setKeyMax(uint8_t key, uint16_t rawMax) {
  if (key < 1U || key > 6U) {
    return false;
  }

  Thresholds next = thresholds_;
  next.keyMax[key - 1U] = rawMax;
  return setThresholds(next);
}

bool KeypadAnalog::setReleaseThreshold(uint16_t rawMax) {
  Thresholds next = thresholds_;
  next.releaseThreshold = rawMax;
  return setThresholds(next);
}

bool KeypadAnalog::setThresholds(const Thresholds& thresholds) {
  if (!isThresholdsValid(thresholds)) {
    return false;
  }
  thresholds_ = thresholds;
  return true;
}

KeypadAnalog::Thresholds KeypadAnalog::defaultThresholds() {
  Thresholds thresholds;
  thresholds.releaseThreshold = config::kKeysReleaseThreshold;
  thresholds.keyMax[0] = config::kKey1Max;
  thresholds.keyMax[1] = config::kKey2Max;
  thresholds.keyMax[2] = config::kKey3Max;
  thresholds.keyMax[3] = config::kKey4Max;
  thresholds.keyMax[4] = config::kKey5Max;
  thresholds.keyMax[5] = config::kKey6Max;
  return thresholds;
}

bool KeypadAnalog::isThresholdsValid(const Thresholds& thresholds) {
  for (uint8_t i = 1; i < 6U; ++i) {
    if (thresholds.keyMax[i] <= thresholds.keyMax[i - 1U]) {
      return false;
    }
  }
  return thresholds.releaseThreshold > thresholds.keyMax[5];
}

uint8_t KeypadAnalog::decodeKey(uint16_t raw) const {
  if (raw > thresholds_.releaseThreshold) {
    return 0;
  }
  if (raw <= thresholds_.keyMax[0]) {
    return 1;
  }
  if (raw <= thresholds_.keyMax[1]) {
    return 2;
  }
  if (raw <= thresholds_.keyMax[2]) {
    return 3;
  }
  if (raw <= thresholds_.keyMax[3]) {
    return 4;
  }
  if (raw <= thresholds_.keyMax[4]) {
    return 5;
  }
  if (raw <= thresholds_.keyMax[5]) {
    return 6;
  }
  return 0;
}
