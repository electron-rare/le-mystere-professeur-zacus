#include "keypad_analog.h"

#include "config.h"

KeypadAnalog::KeypadAnalog(uint8_t adcPin) : adcPin_(adcPin) {}

void KeypadAnalog::begin() {
  analogReadResolution(12);
  analogSetPinAttenuation(adcPin_, ADC_11db);
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

uint8_t KeypadAnalog::decodeKey(uint16_t raw) const {
  if (raw > config::kKeysReleaseThreshold) {
    return 0;
  }
  if (raw <= config::kKey1Max) {
    return 1;
  }
  if (raw <= config::kKey2Max) {
    return 2;
  }
  if (raw <= config::kKey3Max) {
    return 3;
  }
  if (raw <= config::kKey4Max) {
    return 4;
  }
  if (raw <= config::kKey5Max) {
    return 5;
  }
  if (raw <= config::kKey6Max) {
    return 6;
  }
  return 0;
}
