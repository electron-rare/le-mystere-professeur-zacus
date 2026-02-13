#pragma once

#include <Arduino.h>

namespace audio_tools {
class I2SStream;
}
struct FmRadioScanSynth;

class FmRadioScanFx {
 public:
  FmRadioScanFx(uint8_t bclkPin, uint8_t wsPin, uint8_t doutPin, uint8_t i2sPort);
  ~FmRadioScanFx() = default;

  void setGain(float gain);
  void setSampleRate(uint32_t sampleRateHz);

  bool start();
  void stop();
  void update(uint32_t nowMs, uint16_t chunkMs);
  bool isActive() const;

  bool playBlocking(uint32_t durationMs, uint16_t chunkMs = 22U);

 private:
  bool writeFrameBuffer(const int16_t* interleavedStereo, size_t frameCount);
  void resetSynthesisState();
  int16_t nextSample();

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

  float sweepLfoPhase_ = 0.0f;
  float driftLfoPhase_ = 0.0f;
  float noiseLp_ = 0.0f;
  float crackle_ = 0.0f;
  float stationBlend_ = 0.0f;
  uint32_t sweepCycle_ = 0;
  uint32_t sweepPosInCycle_ = 0;
  uint32_t sampleClock_ = 0;
};
