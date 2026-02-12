#include "la_detector.h"

LaDetector::LaDetector(uint8_t micPin) : micPin_(micPin) {}

void LaDetector::begin() {
  analogReadResolution(12);
  analogSetPinAttenuation(micPin_, ADC_11db);
}

void LaDetector::update(uint32_t nowMs) {
  if (!captureInProgress_) {
    if ((nowMs - lastDetectMs_) < config::kDetectEveryMs) {
      return;
    }

    lastDetectMs_ = nowMs;
    captureInProgress_ = true;
    sampleIndex_ = 0;
    nextSampleUs_ = micros();
  }

  const uint32_t nowUs = micros();
  uint8_t samplesRead = 0;

  while (sampleIndex_ < config::kDetectN &&
         static_cast<int32_t>(nowUs - nextSampleUs_) >= 0 &&
         samplesRead < config::kDetectMaxSamplesPerLoop) {
    samples_[sampleIndex_++] = static_cast<int16_t>(analogRead(micPin_));
    nextSampleUs_ += config::kDetectSamplePeriodUs;
    ++samplesRead;
  }

  if (sampleIndex_ < config::kDetectN) {
    return;
  }

  captureInProgress_ = false;
  detected_ = detect(samples_,
                     &targetRatio_,
                     &tuningOffset_,
                     &tuningConfidence_,
                     &micMean_,
                     &micRms_,
                     &micMin_,
                     &micMax_);
}

bool LaDetector::isDetected() const {
  return detected_;
}

int8_t LaDetector::tuningOffset() const {
  return tuningOffset_;
}

uint8_t LaDetector::tuningConfidence() const {
  return tuningConfidence_;
}

float LaDetector::targetRatio() const {
  return targetRatio_;
}

float LaDetector::micMean() const {
  return micMean_;
}

float LaDetector::micRms() const {
  return micRms_;
}

uint16_t LaDetector::micMin() const {
  return micMin_;
}

uint16_t LaDetector::micMax() const {
  return micMax_;
}

uint16_t LaDetector::micPeakToPeak() const {
  return static_cast<uint16_t>(micMax_ - micMin_);
}

float LaDetector::goertzelPower(const int16_t* x, uint16_t n, float fs, float targetHz) const {
  const float k = roundf((static_cast<float>(n) * targetHz) / fs);
  const float omega = (2.0f * PI * k) / static_cast<float>(n);
  const float coeff = 2.0f * cosf(omega);

  float s0 = 0.0f;
  float s1 = 0.0f;
  float s2 = 0.0f;

  for (uint16_t i = 0; i < n; ++i) {
    s0 = x[i] + coeff * s1 - s2;
    s2 = s1;
    s1 = s0;
  }

  return (s1 * s1) + (s2 * s2) - (coeff * s1 * s2);
}

bool LaDetector::detect(const int16_t* samples,
                        float* targetRatio,
                        int8_t* tuningOffset,
                        uint8_t* tuningConfidence,
                        float* micMean,
                        float* micRms,
                        uint16_t* micMin,
                        uint16_t* micMax) const {
  int32_t meanAccum = 0;
  int16_t rawMin = 4095;
  int16_t rawMax = 0;
  for (uint16_t i = 0; i < config::kDetectN; ++i) {
    meanAccum += samples[i];
    if (samples[i] < rawMin) {
      rawMin = samples[i];
    }
    if (samples[i] > rawMax) {
      rawMax = samples[i];
    }
  }

  const float mean = static_cast<float>(meanAccum) / static_cast<float>(config::kDetectN);
  float totalEnergy = 0.0f;
  int16_t centeredSamples[config::kDetectN];

  for (uint16_t i = 0; i < config::kDetectN; ++i) {
    const int16_t centered = static_cast<int16_t>(samples[i] - mean);
    centeredSamples[i] = centered;
    totalEnergy += static_cast<float>(centered) * static_cast<float>(centered);
  }

  const float rms = sqrtf(totalEnergy / static_cast<float>(config::kDetectN));
  if (micMean != nullptr) {
    *micMean = mean;
  }
  if (micRms != nullptr) {
    *micRms = rms;
  }
  if (micMin != nullptr) {
    *micMin = static_cast<uint16_t>(rawMin);
  }
  if (micMax != nullptr) {
    *micMax = static_cast<uint16_t>(rawMax);
  }

  if (totalEnergy < 1.0f) {
    if (targetRatio != nullptr) {
      *targetRatio = 0.0f;
    }
    if (tuningOffset != nullptr) {
      *tuningOffset = 0;
    }
    if (tuningConfidence != nullptr) {
      *tuningConfidence = 0;
    }
    return false;
  }

  const float targetEnergy =
      goertzelPower(centeredSamples, config::kDetectN, config::kDetectFs, config::kDetectTargetHz);
  const float lowEnergy =
      goertzelPower(centeredSamples, config::kDetectN, config::kDetectFs, config::kDetectTargetHz - 20.0f);
  const float highEnergy =
      goertzelPower(centeredSamples, config::kDetectN, config::kDetectFs, config::kDetectTargetHz + 20.0f);

  const float ratio = targetEnergy / (totalEnergy + 1.0f);
  const float sideSum = lowEnergy + highEnergy + 1.0f;
  const float direction = (highEnergy - lowEnergy) / sideSum;

  int8_t offset = static_cast<int8_t>(roundf(direction * 8.0f));
  if (offset < -8) {
    offset = -8;
  } else if (offset > 8) {
    offset = 8;
  }

  uint8_t confidence = 0;
  const float confidenceF = (ratio / config::kDetectRatioThreshold) * 100.0f;
  if (confidenceF <= 0.0f) {
    confidence = 0;
  } else if (confidenceF >= 100.0f) {
    confidence = 100;
  } else {
    confidence = static_cast<uint8_t>(confidenceF);
  }

  if (confidence < 5) {
    offset = 0;
  }

  if (targetRatio != nullptr) {
    *targetRatio = ratio;
  }
  if (tuningOffset != nullptr) {
    *tuningOffset = offset;
  }
  if (tuningConfidence != nullptr) {
    *tuningConfidence = confidence;
  }

  return ratio > config::kDetectRatioThreshold;
}
