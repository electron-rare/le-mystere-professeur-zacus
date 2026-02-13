#pragma once

#include <Arduino.h>
#include <AudioOutputI2S.h>

#include "effects/audio_effect_id.h"

using Mp3FxEffect = AudioEffectId;

enum class Mp3FxMode : uint8_t {
  kDucking = 0,
  kOverlay,
};

class Mp3FxOverlayOutput : public AudioOutputI2S {
 public:
  Mp3FxOverlayOutput(int port = 0, int output_mode = EXTERNAL_I2S, int dma_buf_count = 8, int use_apll = APLL_DISABLE);

  bool SetRate(int hz) override;
  bool ConsumeSample(int16_t sample[2]) override;

  void setFxMode(Mp3FxMode mode);
  Mp3FxMode fxMode() const;

  void setDuckingGain(float gain);
  float duckingGain() const;

  void setOverlayGain(float gain);
  float overlayGain() const;

  bool triggerFx(Mp3FxEffect effect, uint32_t durationMs);
  void stopFx();
  bool isFxActive() const;
  Mp3FxEffect activeFx() const;
  uint32_t fxRemainingMs() const;

 private:
  int16_t nextFxSample();
  int16_t nextFmSample();
  int16_t nextSonarSample();
  int16_t nextMorseSample();
  int16_t nextWinSample();

  bool prepareMorseState();
  bool prepareWinState();
  static float clampf(float value, float minValue, float maxValue);
  static int16_t clamp16(int32_t value);

  static constexpr char kMorsePattern_[] = ".-- .. -.";  // WIN

  uint32_t sampleRateHz_ = 44100U;
  Mp3FxMode mode_ = Mp3FxMode::kDucking;
  float duckingGain_ = 0.45f;
  float overlayGain_ = 0.42f;

  bool fxActive_ = false;
  Mp3FxEffect fxEffect_ = Mp3FxEffect::kFmSweep;
  uint32_t fxRemainingSamples_ = 0;
  uint32_t fxSampleClock_ = 0;

  float fmPhaseA_ = 0.0f;
  float fmPhaseB_ = 0.0f;
  float fmNoiseLp_ = 0.0f;

  float sonarPhase_ = 0.0f;
  float sonarEchoPhase_ = 0.0f;

  float morsePhase_ = 0.0f;
  uint32_t morseToneSamplesLeft_ = 0;
  uint32_t morseGapSamplesLeft_ = 0;
  uint16_t morsePatternPos_ = 0;

  float winPhase_ = 0.0f;
  uint32_t winStepSamplesLeft_ = 0;
  uint32_t winStepTotalSamples_ = 0;
  uint16_t winStepIndex_ = 0;
  uint16_t winCurrentFreqHz_ = 0;
};
