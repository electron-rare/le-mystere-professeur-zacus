#pragma once

#include <Arduino.h>

#include "effects/audio_effect_id.h"

namespace audio_tools {
class I2SStream;
}
struct FmRadioScanSynth;

class FmRadioScanFx {
 public:
  using Effect = AudioEffectId;

  FmRadioScanFx(uint8_t bclkPin, uint8_t wsPin, uint8_t doutPin, uint8_t i2sPort);
  ~FmRadioScanFx() = default;

  void setGain(float gain);
  void setSampleRate(uint32_t sampleRateHz);
  void setEffect(Effect effect);
  Effect effect() const;

  bool start(Effect effect);
  bool start();
  void stop();
  void update(uint32_t nowMs, uint16_t chunkMs);
  bool isActive() const;

  bool playBlocking(Effect effect, uint32_t durationMs, uint16_t chunkMs = 22U);
  bool playBlocking(uint32_t durationMs, uint16_t chunkMs = 22U);

 private:
  bool writeFrameBuffer(const int16_t* interleavedStereo, size_t frameCount);
  void resetSynthesisState();
  int16_t nextSample();
  int16_t nextSampleFmSweep();
  int16_t nextSampleSonar();
  int16_t nextSampleMorse();
  int16_t nextSampleWin();
  bool morsePrepareNextState();
  bool winPrepareNextStep();

  static constexpr uint16_t kSynthRateHz = 22050U;

  uint8_t bclkPin_;
  uint8_t wsPin_;
  uint8_t doutPin_;
  uint8_t i2sPort_;

  audio_tools::I2SStream* i2sStream_ = nullptr;
  FmRadioScanSynth* synth_ = nullptr;
  bool active_ = false;
  float gain_ = 0.18f;
  uint32_t sampleRateHz_ = 22050U;
  Effect effect_ = Effect::kFmSweep;

  float sweepLfoPhase_ = 0.0f;
  float driftLfoPhase_ = 0.0f;
  float noiseLp_ = 0.0f;
  float crackle_ = 0.0f;
  float stationBlend_ = 0.0f;
  uint32_t sweepCycle_ = 0;
  uint32_t sweepPosInCycle_ = 0;
  uint32_t sampleClock_ = 0;
  float sonarPhase_ = 0.0f;
  float sonarEchoPhase_ = 0.0f;
  float morsePhase_ = 0.0f;
  float winPhase_ = 0.0f;
  uint32_t morseToneSamplesLeft_ = 0;
  uint32_t morseGapSamplesLeft_ = 0;
  uint16_t morsePatternPos_ = 0;
  uint32_t winStepSamplesLeft_ = 0;
  uint32_t winStepTotalSamples_ = 0;
  uint16_t winStepIndex_ = 0;
  uint16_t winCurrentFreqHz_ = 0;
};
