#include "mp3_fx_overlay_output.h"

#include <cmath>

namespace {

constexpr float kTwoPi = 6.28318530718f;

constexpr uint16_t kMorseUnitMs = 90U;
constexpr uint16_t kMorseFreqHz = 680U;

constexpr uint16_t kWinNotesHz[] = {
    523U, 659U, 784U, 1047U, 1319U, 1047U, 1568U, 1319U, 0U};
constexpr uint16_t kWinNotesMs[] = {
    120U, 120U, 120U, 150U, 180U, 120U, 210U, 260U, 180U};
constexpr uint16_t kWinNoteCount = sizeof(kWinNotesHz) / sizeof(kWinNotesHz[0]);

}  // namespace

constexpr char Mp3FxOverlayOutput::kMorsePattern_[];

Mp3FxOverlayOutput::Mp3FxOverlayOutput(int port, int output_mode, int dma_buf_count, int use_apll)
    : AudioOutputI2S(port, output_mode, dma_buf_count, use_apll) {}

bool Mp3FxOverlayOutput::SetRate(int hz) {
  if (hz > 0) {
    sampleRateHz_ = static_cast<uint32_t>(hz);
  }
  return AudioOutputI2S::SetRate(hz);
}

bool Mp3FxOverlayOutput::ConsumeSample(int16_t sample[2]) {
  if (!fxActive_) {
    return AudioOutputI2S::ConsumeSample(sample);
  }

  int32_t left = sample[0];
  int32_t right = sample[1];

  if (mode_ == Mp3FxMode::kDucking) {
    left = static_cast<int32_t>(static_cast<float>(left) * duckingGain_);
    right = static_cast<int32_t>(static_cast<float>(right) * duckingGain_);
  }

  const int16_t fxSample = nextFxSample();
  if (fxActive_ || fxSample != 0) {
    const int32_t fxMixed = static_cast<int32_t>(static_cast<float>(fxSample) * overlayGain_);
    left += fxMixed;
    right += fxMixed;
  }

  int16_t mixed[2] = {clamp16(left), clamp16(right)};
  return AudioOutputI2S::ConsumeSample(mixed);
}

void Mp3FxOverlayOutput::setFxMode(Mp3FxMode mode) {
  mode_ = mode;
}

Mp3FxMode Mp3FxOverlayOutput::fxMode() const {
  return mode_;
}

void Mp3FxOverlayOutput::setDuckingGain(float gain) {
  duckingGain_ = clampf(gain, 0.0f, 1.0f);
}

float Mp3FxOverlayOutput::duckingGain() const {
  return duckingGain_;
}

void Mp3FxOverlayOutput::setOverlayGain(float gain) {
  overlayGain_ = clampf(gain, 0.0f, 1.0f);
}

float Mp3FxOverlayOutput::overlayGain() const {
  return overlayGain_;
}

bool Mp3FxOverlayOutput::triggerFx(Mp3FxEffect effect, uint32_t durationMs) {
  if (durationMs == 0U || sampleRateHz_ == 0U) {
    return false;
  }

  fxEffect_ = effect;
  fxRemainingSamples_ = (sampleRateHz_ * durationMs) / 1000UL;
  if (fxRemainingSamples_ == 0U) {
    fxRemainingSamples_ = 1U;
  }
  fxSampleClock_ = 0U;

  fmPhaseA_ = 0.0f;
  fmPhaseB_ = 0.0f;
  fmNoiseLp_ = 0.0f;
  sonarPhase_ = 0.0f;
  sonarEchoPhase_ = 0.0f;
  morsePhase_ = 0.0f;
  morseToneSamplesLeft_ = 0U;
  morseGapSamplesLeft_ = 0U;
  morsePatternPos_ = 0U;
  winPhase_ = 0.0f;
  winStepSamplesLeft_ = 0U;
  winStepTotalSamples_ = 0U;
  winStepIndex_ = 0U;
  winCurrentFreqHz_ = 0U;

  if (effect == Mp3FxEffect::kMorse) {
    prepareMorseState();
  } else if (effect == Mp3FxEffect::kWin) {
    prepareWinState();
  }

  fxActive_ = true;
  return true;
}

void Mp3FxOverlayOutput::stopFx() {
  fxActive_ = false;
  fxRemainingSamples_ = 0U;
}

bool Mp3FxOverlayOutput::isFxActive() const {
  return fxActive_;
}

Mp3FxEffect Mp3FxOverlayOutput::activeFx() const {
  return fxEffect_;
}

uint32_t Mp3FxOverlayOutput::fxRemainingMs() const {
  if (!fxActive_ || sampleRateHz_ == 0U) {
    return 0U;
  }
  return (fxRemainingSamples_ * 1000UL) / sampleRateHz_;
}

int16_t Mp3FxOverlayOutput::nextFxSample() {
  if (!fxActive_ || fxRemainingSamples_ == 0U || sampleRateHz_ == 0U) {
    fxActive_ = false;
    return 0;
  }

  int16_t sample = 0;
  switch (fxEffect_) {
    case Mp3FxEffect::kSonar:
      sample = nextSonarSample();
      break;
    case Mp3FxEffect::kMorse:
      sample = nextMorseSample();
      break;
    case Mp3FxEffect::kWin:
      sample = nextWinSample();
      break;
    case Mp3FxEffect::kFmSweep:
    default:
      sample = nextFmSample();
      break;
  }

  ++fxSampleClock_;
  --fxRemainingSamples_;
  if (fxRemainingSamples_ == 0U) {
    fxActive_ = false;
  }
  return sample;
}

int16_t Mp3FxOverlayOutput::nextFmSample() {
  const uint32_t sweepPeriodSamples = (sampleRateHz_ * 2600UL) / 1000UL;
  float sweepT = 0.0f;
  if (sweepPeriodSamples > 0U) {
    sweepT = static_cast<float>(fxSampleClock_ % sweepPeriodSamples) /
             static_cast<float>(sweepPeriodSamples);
    if (((fxSampleClock_ / sweepPeriodSamples) & 1U) != 0U) {
      sweepT = 1.0f - sweepT;
    }
  }

  const bool stationWindow = (sweepT > 0.20f && sweepT < 0.34f) || (sweepT > 0.58f && sweepT < 0.74f);
  const float sweepHz = stationWindow ? (240.0f + (130.0f * sinf(kTwoPi * sweepT * 2.0f)))
                                      : (95.0f + (1300.0f * sweepT));
  const float carrierHz = stationWindow ? (560.0f + (120.0f * sinf(fmPhaseB_)))
                                        : (760.0f + (280.0f * sinf(fmPhaseB_)));

  fmPhaseA_ += kTwoPi * (sweepHz / static_cast<float>(sampleRateHz_));
  if (fmPhaseA_ >= kTwoPi) {
    fmPhaseA_ -= kTwoPi;
  }
  fmPhaseB_ += kTwoPi * (carrierHz / static_cast<float>(sampleRateHz_));
  if (fmPhaseB_ >= kTwoPi) {
    fmPhaseB_ -= kTwoPi;
  }

  const float rawNoise = static_cast<float>(random(-128L, 128L)) / 128.0f;
  fmNoiseLp_ = (0.985f * fmNoiseLp_) + (0.015f * rawNoise);
  const float hiss = rawNoise - fmNoiseLp_;

  float sampleF = 0.0f;
  sampleF += stationWindow ? 0.28f * sinf(fmPhaseA_) : 0.45f * sinf(fmPhaseA_);
  sampleF += stationWindow ? 0.20f * sinf(fmPhaseB_) : 0.15f * sinf(fmPhaseB_);
  sampleF += stationWindow ? 0.16f * hiss : 0.32f * hiss;

  sampleF = clampf(sampleF, -1.0f, 1.0f);
  return static_cast<int16_t>(sampleF * 28000.0f);
}

int16_t Mp3FxOverlayOutput::nextSonarSample() {
  const uint32_t periodSamples = (sampleRateHz_ * 1200UL) / 1000UL;
  const uint32_t pingSamples = (sampleRateHz_ * 130UL) / 1000UL;
  const uint32_t echoStartSamples = (sampleRateHz_ * 200UL) / 1000UL;
  const uint32_t echoLenSamples = (sampleRateHz_ * 420UL) / 1000UL;

  const uint32_t cycle = (periodSamples > 0U) ? (fxSampleClock_ % periodSamples) : 0U;
  float sampleF = 0.0f;

  if (cycle < pingSamples && pingSamples > 0U) {
    const float pingT = static_cast<float>(cycle) / static_cast<float>(pingSamples);
    const float freqHz = 1800.0f - (1300.0f * pingT);
    sonarPhase_ += kTwoPi * (freqHz / static_cast<float>(sampleRateHz_));
    if (sonarPhase_ >= kTwoPi) {
      sonarPhase_ -= kTwoPi;
    }
    const float env = (1.0f - pingT) * (1.0f - pingT);
    sampleF += 0.92f * sinf(sonarPhase_) * env;
  }

  if (cycle >= echoStartSamples && cycle < (echoStartSamples + echoLenSamples) && echoLenSamples > 0U) {
    const uint32_t echoPos = cycle - echoStartSamples;
    const float echoT = static_cast<float>(echoPos) / static_cast<float>(echoLenSamples);
    const float freqHz = 680.0f - (220.0f * echoT);
    sonarEchoPhase_ += kTwoPi * (freqHz / static_cast<float>(sampleRateHz_));
    if (sonarEchoPhase_ >= kTwoPi) {
      sonarEchoPhase_ -= kTwoPi;
    }
    const float env = expf(-4.0f * echoT);
    sampleF += 0.46f * sinf(sonarEchoPhase_) * env;
  }

  sampleF = clampf(sampleF, -1.0f, 1.0f);
  return static_cast<int16_t>(sampleF * 30000.0f);
}

bool Mp3FxOverlayOutput::prepareMorseState() {
  const uint32_t unitSamplesRaw = (sampleRateHz_ * static_cast<uint32_t>(kMorseUnitMs)) / 1000UL;
  const uint32_t unitSamples = (unitSamplesRaw > 0U) ? unitSamplesRaw : 1U;

  while (true) {
    const char symbol = kMorsePattern_[morsePatternPos_];
    if (symbol == '\0') {
      morsePatternPos_ = 0U;
      morseGapSamplesLeft_ = unitSamples * 7U;
      return false;
    }
    ++morsePatternPos_;

    if (symbol == ' ') {
      morseGapSamplesLeft_ = unitSamples * 3U;
      return false;
    }

    if (symbol == '.') {
      morseToneSamplesLeft_ = unitSamples;
      morseGapSamplesLeft_ = unitSamples;
      return true;
    }

    if (symbol == '-') {
      morseToneSamplesLeft_ = unitSamples * 3U;
      morseGapSamplesLeft_ = unitSamples;
      return true;
    }
  }
}

int16_t Mp3FxOverlayOutput::nextMorseSample() {
  if (morseToneSamplesLeft_ == 0U) {
    if (morseGapSamplesLeft_ > 0U) {
      --morseGapSamplesLeft_;
      return 0;
    }
    if (!prepareMorseState()) {
      return 0;
    }
  }

  const float warble = 1.0f +
                       (0.05f * sinf(kTwoPi * 0.8f *
                                     (static_cast<float>(fxSampleClock_) /
                                      static_cast<float>(sampleRateHz_))));
  const float freqHz = static_cast<float>(kMorseFreqHz) * warble;
  morsePhase_ += kTwoPi * (freqHz / static_cast<float>(sampleRateHz_));
  if (morsePhase_ >= kTwoPi) {
    morsePhase_ -= kTwoPi;
  }

  float sampleF = 0.80f * sinf(morsePhase_);
  sampleF += 0.10f * sinf(morsePhase_ * 2.0f);
  sampleF = clampf(sampleF, -1.0f, 1.0f);

  --morseToneSamplesLeft_;
  return static_cast<int16_t>(sampleF * 30000.0f);
}

bool Mp3FxOverlayOutput::prepareWinState() {
  if (kWinNoteCount == 0U) {
    return false;
  }

  if (winStepIndex_ >= kWinNoteCount) {
    winStepIndex_ = 0U;
  }

  const uint16_t idx = winStepIndex_;
  winCurrentFreqHz_ = kWinNotesHz[idx];
  uint32_t stepSamples =
      (sampleRateHz_ * static_cast<uint32_t>(kWinNotesMs[idx])) / 1000UL;
  if (stepSamples == 0U) {
    stepSamples = 1U;
  }
  winStepSamplesLeft_ = stepSamples;
  winStepTotalSamples_ = stepSamples;
  ++winStepIndex_;
  return true;
}

int16_t Mp3FxOverlayOutput::nextWinSample() {
  if (winStepSamplesLeft_ == 0U && !prepareWinState()) {
    return 0;
  }

  float sampleF = 0.0f;
  if (winCurrentFreqHz_ > 0U) {
    winPhase_ += kTwoPi * (static_cast<float>(winCurrentFreqHz_) /
                           static_cast<float>(sampleRateHz_));
    if (winPhase_ >= kTwoPi) {
      winPhase_ -= kTwoPi;
    }

    const float sineWave = sinf(winPhase_);
    const float squareWave = (sineWave >= 0.0f) ? 1.0f : -1.0f;
    const float progress =
        1.0f - (static_cast<float>(winStepSamplesLeft_) /
                static_cast<float>(winStepTotalSamples_));
    float env = 1.0f - (0.70f * progress);

    const uint32_t attackSamplesRaw = (sampleRateHz_ * 5UL) / 1000UL;
    const uint32_t releaseSamplesRaw = (sampleRateHz_ * 18UL) / 1000UL;
    const uint32_t attackSamples = (attackSamplesRaw > 0U) ? attackSamplesRaw : 1U;
    const uint32_t releaseSamples = (releaseSamplesRaw > 0U) ? releaseSamplesRaw : 1U;
    if (winStepSamplesLeft_ < releaseSamples) {
      const float releaseEnv = static_cast<float>(winStepSamplesLeft_) /
                               static_cast<float>(releaseSamples);
      if (releaseEnv < env) {
        env = releaseEnv;
      }
    }
    const uint32_t elapsedSamples = winStepTotalSamples_ - winStepSamplesLeft_;
    if (elapsedSamples < attackSamples) {
      const float attackEnv = static_cast<float>(elapsedSamples) /
                              static_cast<float>(attackSamples);
      if (attackEnv < env) {
        env = attackEnv;
      }
    }

    sampleF = (0.70f * sineWave) + (0.30f * squareWave);
    sampleF += 0.18f * sinf(winPhase_ * 1.5f);
    sampleF *= env;
  }

  if (winStepSamplesLeft_ > 0U) {
    --winStepSamplesLeft_;
  }

  sampleF = clampf(sampleF, -1.0f, 1.0f);
  return static_cast<int16_t>(sampleF * 30000.0f);
}

float Mp3FxOverlayOutput::clampf(float value, float minValue, float maxValue) {
  if (value < minValue) {
    return minValue;
  }
  if (value > maxValue) {
    return maxValue;
  }
  return value;
}

int16_t Mp3FxOverlayOutput::clamp16(int32_t value) {
  if (value < -32767) {
    return -32767;
  }
  if (value > 32767) {
    return 32767;
  }
  return static_cast<int16_t>(value);
}
