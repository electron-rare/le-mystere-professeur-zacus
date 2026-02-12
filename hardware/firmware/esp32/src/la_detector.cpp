#include "la_detector.h"

LaDetector::LaDetector(uint8_t micAdcPin,
                       bool useI2sMic,
                       uint8_t i2sBclkPin,
                       uint8_t i2sWsPin,
                       uint8_t i2sDinPin)
    : micAdcPin_(micAdcPin),
      useI2sMic_(useI2sMic),
      i2sBclkPin_(i2sBclkPin),
      i2sWsPin_(i2sWsPin),
      i2sDinPin_(i2sDinPin),
      codec_(config::kPinCodecI2CSda,
             config::kPinCodecI2CScl,
             config::kCodecI2CClockHz,
             config::kCodecI2CAddress,
             i2sBclkPin,
             i2sWsPin,
             config::kPinI2SDout,
             i2sDinPin,
             config::kI2sOutputPort,
             config::kPinAudioPaEnable) {}

void LaDetector::begin() {
  if (!useI2sMic_) {
    analogReadResolution(12);
    analogSetPinAttenuation(micAdcPin_, ADC_11db);
    return;
  }
  beginI2sInput();
}

void LaDetector::setCaptureEnabled(bool enabled) {
  if (captureEnabled_ == enabled) {
    return;
  }

  captureEnabled_ = enabled;
  captureInProgress_ = false;
  sampleIndex_ = 0;

  if (!useI2sMic_) {
    return;
  }

  if (captureEnabled_) {
    beginI2sInput();
  } else {
    endI2sInput();
  }
}

bool LaDetector::beginI2sInput() {
  if (i2sReady_) {
    return true;
  }

  if (!codec_.isReady() && !beginCodec()) {
    return false;
  }

  i2s_config_t i2sConfig = {};
  i2sConfig.mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_RX);
  i2sConfig.sample_rate = static_cast<int>(config::kDetectFs);
  i2sConfig.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
  i2sConfig.channel_format = config::kMicI2SUseLeftChannel ? I2S_CHANNEL_FMT_ONLY_LEFT
                                                            : I2S_CHANNEL_FMT_ONLY_RIGHT;
  i2sConfig.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  i2sConfig.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
  i2sConfig.dma_buf_count = 4;
  i2sConfig.dma_buf_len = 64;
  i2sConfig.use_apll = false;
  i2sConfig.tx_desc_auto_clear = false;
  i2sConfig.fixed_mclk = 0;

  const esp_err_t installErr = i2s_driver_install(i2sPort_, &i2sConfig, 0, nullptr);
  if (installErr != ESP_OK) {
    Serial.printf("[MIC][I2S] driver install failed err=%d\n", static_cast<int>(installErr));
    return false;
  }

  i2s_pin_config_t pinConfig = {};
  pinConfig.bck_io_num = static_cast<int>(i2sBclkPin_);
  pinConfig.ws_io_num = static_cast<int>(i2sWsPin_);
  pinConfig.data_out_num = I2S_PIN_NO_CHANGE;
  pinConfig.data_in_num = static_cast<int>(i2sDinPin_);

  const esp_err_t pinErr = i2s_set_pin(i2sPort_, &pinConfig);
  if (pinErr != ESP_OK) {
    i2s_driver_uninstall(i2sPort_);
    Serial.printf("[MIC][I2S] pin config failed err=%d\n", static_cast<int>(pinErr));
    return false;
  }

  const esp_err_t clkErr = i2s_set_clk(i2sPort_,
                                       static_cast<int>(config::kDetectFs),
                                       I2S_BITS_PER_SAMPLE_16BIT,
                                       I2S_CHANNEL_MONO);
  if (clkErr != ESP_OK) {
    i2s_driver_uninstall(i2sPort_);
    Serial.printf("[MIC][I2S] clock config failed err=%d\n", static_cast<int>(clkErr));
    return false;
  }

  i2sReady_ = true;
  Serial.println("[MIC][I2S] RX ready.");
  return true;
}

void LaDetector::endI2sInput() {
  if (!i2sReady_) {
    return;
  }
  i2s_driver_uninstall(i2sPort_);
  i2sReady_ = false;
}

bool LaDetector::configureCodecInput(bool useLine2) {
  if (!codec_.configureInput(useLine2, config::kCodecMicGainDb)) {
    Serial.printf("[MIC][CODEC] input config failed (LINE%u).\n", useLine2 ? 2U : 1U);
    return false;
  }

  codecUseLine2_ = useLine2;
  Serial.printf("[MIC][CODEC] input LINE%u active, mic_gain=%u dB.\n",
                codecUseLine2_ ? 2U : 1U,
                static_cast<unsigned int>(config::kCodecMicGainDb));
  return true;
}

bool LaDetector::beginCodec() {
  if (!codec_.begin(codecUseLine2_, config::kCodecMicGainDb)) {
    const uint8_t altAddress = (config::kCodecI2CAddress == 0x10U) ? 0x11U : 0x10U;
    Serial.printf("[MIC][CODEC] ES8388 introuvable sur I2C (0x%02X/0x%02X, SDA=%u, SCL=%u)\n",
                  static_cast<unsigned int>(config::kCodecI2CAddress),
                  static_cast<unsigned int>(altAddress),
                  static_cast<unsigned int>(config::kPinCodecI2CSda),
                  static_cast<unsigned int>(config::kPinCodecI2CScl));
    return false;
  }

  Serial.printf("[MIC][CODEC] ES8388 detecte addr=0x%02X (SDA=%u SCL=%u)\n",
                static_cast<unsigned int>(codec_.address()),
                static_cast<unsigned int>(config::kPinCodecI2CSda),
                static_cast<unsigned int>(config::kPinCodecI2CScl));

  if (!configureCodecInput(codecUseLine2_)) {
    return false;
  }

  codecAutoSwitched_ = false;
  codecSilenceSinceMs_ = 0;
  Serial.println("[MIC][CODEC] init OK (arduino-audio-driver).");
  return true;
}

void LaDetector::maybeAutoSwitchCodecInput(uint32_t nowMs) {
  if (!useI2sMic_ || !codec_.isReady() || !config::kCodecMicAutoSwitchLineOnSilence ||
      codecAutoSwitched_) {
    return;
  }

  const uint16_t p2p = micPeakToPeak();
  if (p2p > config::kCodecMicSilenceP2PThreshold) {
    codecSilenceSinceMs_ = 0;
    return;
  }

  if (codecSilenceSinceMs_ == 0) {
    codecSilenceSinceMs_ = nowMs;
    return;
  }

  if ((nowMs - codecSilenceSinceMs_) < config::kCodecMicSilenceSwitchMs) {
    return;
  }

  const bool nextUseLine2 = !codecUseLine2_;
  Serial.printf("[MIC][CODEC] silence persistant (%u ms, p2p=%u): tentative LINE%u -> LINE%u\n",
                static_cast<unsigned int>(config::kCodecMicSilenceSwitchMs),
                static_cast<unsigned int>(p2p),
                codecUseLine2_ ? 2U : 1U,
                nextUseLine2 ? 2U : 1U);
  if (configureCodecInput(nextUseLine2)) {
    codecAutoSwitched_ = true;
    codecSilenceSinceMs_ = 0;
  } else {
    codecSilenceSinceMs_ = nowMs;
  }
}

void LaDetector::captureFromAdc() {
  const uint32_t nowUs = micros();
  uint8_t samplesRead = 0;

  while (sampleIndex_ < config::kDetectN &&
         static_cast<int32_t>(nowUs - nextSampleUs_) >= 0 &&
         samplesRead < config::kDetectMaxSamplesPerLoop) {
    samples_[sampleIndex_++] = static_cast<int16_t>(analogRead(micAdcPin_));
    nextSampleUs_ += config::kDetectSamplePeriodUs;
    ++samplesRead;
  }
}

void LaDetector::captureFromI2s() {
  int16_t i2sBuffer[32];

  while (sampleIndex_ < config::kDetectN) {
    const size_t remaining = static_cast<size_t>(config::kDetectN - sampleIndex_);
    const size_t requested = (remaining < 32U) ? remaining : 32U;
    size_t bytesRead = 0;
    const esp_err_t readErr =
        i2s_read(i2sPort_, i2sBuffer, requested * sizeof(int16_t), &bytesRead, 0);
    if (readErr != ESP_OK || bytesRead == 0) {
      break;
    }

    const size_t samplesRead = bytesRead / sizeof(int16_t);
    for (size_t i = 0; i < samplesRead && sampleIndex_ < config::kDetectN; ++i) {
      // Convert signed PCM16 to pseudo-ADC 12-bit range [0..4095].
      int32_t normalized = static_cast<int32_t>(i2sBuffer[i]) + 32768;
      if (normalized < 0) {
        normalized = 0;
      } else if (normalized > 65535) {
        normalized = 65535;
      }
      samples_[sampleIndex_++] = static_cast<int16_t>(normalized >> 4);
    }
  }
}

void LaDetector::update(uint32_t nowMs) {
  if (!captureEnabled_) {
    return;
  }

  if (useI2sMic_ && !i2sReady_ && !beginI2sInput()) {
    return;
  }

  if (!captureInProgress_) {
    if ((nowMs - lastDetectMs_) < config::kDetectEveryMs) {
      return;
    }

    lastDetectMs_ = nowMs;
    captureInProgress_ = true;
    sampleIndex_ = 0;
    nextSampleUs_ = micros();
  }

  if (useI2sMic_) {
    captureFromI2s();
  } else {
    captureFromAdc();
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
  maybeAutoSwitchCodecInput(nowMs);
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

bool LaDetector::isCodecReady() const {
  return codec_.isReady();
}

uint8_t LaDetector::codecAddress() const {
  return codec_.address();
}

bool LaDetector::ensureCodecReady() {
  return codec_.ensureReady();
}

bool LaDetector::readCodecRegister(uint8_t reg, uint8_t* value) {
  return codec_.readRegister(reg, value);
}

bool LaDetector::writeCodecRegister(uint8_t reg, uint8_t value) {
  return codec_.writeRegister(reg, value);
}

bool LaDetector::setCodecOutputVolumeRaw(uint8_t rawValue, bool includeOut2) {
  return codec_.setOutputVolumeRaw(rawValue, includeOut2);
}

bool LaDetector::setCodecOutputVolumePercent(uint8_t percent, bool includeOut2) {
  return setCodecOutputVolumeRaw(codecOutputRawFromPercent(percent), includeOut2);
}

uint8_t LaDetector::codecOutputRawFromPercent(uint8_t percent) {
  return CodecEs8388Driver::outputRawFromPercent(percent);
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

  const uint16_t p2p = static_cast<uint16_t>(rawMax - rawMin);
  if (p2p < config::kDetectMinP2PForDetection || rms < config::kDetectMinRmsForDetection) {
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
  const float lowEnergy = goertzelPower(centeredSamples,
                                        config::kDetectN,
                                        config::kDetectFs,
                                        config::kDetectTargetHz - 20.0f);
  const float highEnergy = goertzelPower(centeredSamples,
                                         config::kDetectN,
                                         config::kDetectFs,
                                         config::kDetectTargetHz + 20.0f);

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
