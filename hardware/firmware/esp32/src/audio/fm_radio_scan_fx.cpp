#include "fm_radio_scan_fx.h"

#define USE_AUDIOTOOLS_NS false
#include <AudioTools.h>
#include <Oscil.h>
#include <tables/sin2048_int8.h>

#include <cmath>
#include <new>

namespace {

constexpr float kTwoPi = 6.28318530718f;
constexpr uint16_t kBlockFrames = 96U;
constexpr uint16_t kSynthRateHz = 22050U;

constexpr char kMorsePattern[] = ".-- .. -.";  // "WIN"
constexpr uint16_t kMorseUnitMs = 90U;
constexpr uint16_t kMorseFreqHz = 680U;

constexpr uint16_t kWinNotesHz[] = {
    523U, 659U, 784U, 1047U, 1319U, 1047U, 1568U, 1319U, 0U};
constexpr uint16_t kWinNotesMs[] = {
    120U, 120U, 120U, 150U, 180U, 120U, 210U, 260U, 180U};
constexpr uint16_t kWinNoteCount = sizeof(kWinNotesHz) / sizeof(kWinNotesHz[0]);

float clampf(float value, float minValue, float maxValue) {
  if (value < minValue) {
    return minValue;
  }
  if (value > maxValue) {
    return maxValue;
  }
  return value;
}

}  // namespace

struct FmRadioScanSynth {
  Oscil<SIN2048_NUM_CELLS, kSynthRateHz> sweepOsc{SIN2048_DATA};
  Oscil<SIN2048_NUM_CELLS, kSynthRateHz> stationOsc{SIN2048_DATA};
  Oscil<SIN2048_NUM_CELLS, kSynthRateHz> carrierOsc{SIN2048_DATA};
};

FmRadioScanFx::FmRadioScanFx(uint8_t bclkPin, uint8_t wsPin, uint8_t doutPin, uint8_t i2sPort)
    : bclkPin_(bclkPin), wsPin_(wsPin), doutPin_(doutPin), i2sPort_(i2sPort) {}

void FmRadioScanFx::setGain(float gain) {
  gain_ = clampf(gain, 0.0f, 1.0f);
}

void FmRadioScanFx::setSampleRate(uint32_t sampleRateHz) {
  if (sampleRateHz < 8000U) {
    sampleRateHz_ = 8000U;
    return;
  }
  if (sampleRateHz > 48000U) {
    sampleRateHz_ = 48000U;
    return;
  }
  sampleRateHz_ = sampleRateHz;
}

void FmRadioScanFx::setEffect(Effect effect) {
  effect_ = effect;
  if (active_) {
    resetSynthesisState();
  }
}

FmRadioScanFx::Effect FmRadioScanFx::effect() const {
  return effect_;
}

bool FmRadioScanFx::start(Effect effect) {
  setEffect(effect);
  return start();
}

bool FmRadioScanFx::start() {
  stop();

  auto* stream = new (std::nothrow) audio_tools::I2SStream();
  if (stream == nullptr) {
    return false;
  }

  audio_tools::I2SConfig cfg = stream->defaultConfig(audio_tools::TX_MODE);
  cfg.port_no = static_cast<int>(i2sPort_);
  cfg.pin_bck = static_cast<int>(bclkPin_);
  cfg.pin_ws = static_cast<int>(wsPin_);
  cfg.pin_data = static_cast<int>(doutPin_);
  cfg.sample_rate = sampleRateHz_;
  cfg.channels = 2;
  cfg.bits_per_sample = 16;
  cfg.buffer_count = 8;
  cfg.buffer_size = 512;
  cfg.auto_clear = true;
  cfg.use_apll = false;
  if (!stream->begin(cfg)) {
    delete stream;
    return false;
  }

  auto* synth = new (std::nothrow) FmRadioScanSynth();
  if (synth == nullptr) {
    stream->end();
    delete stream;
    return false;
  }

  i2sStream_ = stream;
  synth_ = synth;
  resetSynthesisState();
  randomSeed(static_cast<uint32_t>(micros()));
  active_ = true;
  return true;
}

void FmRadioScanFx::stop() {
  if (i2sStream_ != nullptr) {
    i2sStream_->end();
    delete i2sStream_;
    i2sStream_ = nullptr;
  }
  if (synth_ != nullptr) {
    delete synth_;
    synth_ = nullptr;
  }
  active_ = false;
}

bool FmRadioScanFx::isActive() const {
  return active_;
}

void FmRadioScanFx::update(uint32_t nowMs, uint16_t chunkMs) {
  (void)nowMs;
  if (!active_ || i2sStream_ == nullptr || synth_ == nullptr) {
    return;
  }

  uint32_t chunkSamples = (sampleRateHz_ * static_cast<uint32_t>(chunkMs)) / 1000UL;
  if (chunkSamples < 64U) {
    chunkSamples = 64U;
  } else if (chunkSamples > 1024U) {
    chunkSamples = 1024U;
  }

  int16_t interleaved[kBlockFrames * 2U] = {};
  while (chunkSamples > 0U && active_) {
    const uint16_t blockFrames =
        (chunkSamples > kBlockFrames) ? kBlockFrames : static_cast<uint16_t>(chunkSamples);
    for (uint16_t i = 0; i < blockFrames; ++i) {
      const int16_t sample = nextSample();
      interleaved[(i * 2U)] = sample;
      interleaved[(i * 2U) + 1U] = sample;
    }
    if (!writeFrameBuffer(interleaved, blockFrames)) {
      stop();
      return;
    }
    chunkSamples -= blockFrames;
  }
}

bool FmRadioScanFx::playBlocking(Effect effect, uint32_t durationMs, uint16_t chunkMs) {
  setEffect(effect);
  return playBlocking(durationMs, chunkMs);
}

bool FmRadioScanFx::playBlocking(uint32_t durationMs, uint16_t chunkMs) {
  if (durationMs == 0U) {
    return true;
  }

  const bool wasActive = active_;
  if (!wasActive && !start()) {
    return false;
  }

  const uint32_t deadlineMs = millis() + durationMs;
  while (active_) {
    const uint32_t nowMs = millis();
    if (static_cast<int32_t>(nowMs - deadlineMs) >= 0) {
      break;
    }
    update(nowMs, chunkMs);
    delay(0);
  }

  const bool ok = active_;
  if (!wasActive) {
    stop();
  }
  return ok;
}

bool FmRadioScanFx::writeFrameBuffer(const int16_t* interleavedStereo, size_t frameCount) {
  if (i2sStream_ == nullptr || interleavedStereo == nullptr || frameCount == 0U) {
    return false;
  }

  size_t bytesLeft = frameCount * 2U * sizeof(int16_t);
  const uint8_t* src = reinterpret_cast<const uint8_t*>(interleavedStereo);
  uint8_t waitGuard = 0U;
  while (bytesLeft > 0U) {
    const size_t written = i2sStream_->write(src, bytesLeft);
    if (written == 0U) {
      delayMicroseconds(80);
      ++waitGuard;
      if (waitGuard >= 120U) {
        return false;
      }
      continue;
    }
    src += written;
    bytesLeft -= written;
    waitGuard = 0U;
  }
  return true;
}

void FmRadioScanFx::resetSynthesisState() {
  sweepLfoPhase_ = 0.0f;
  driftLfoPhase_ = 0.0f;
  noiseLp_ = 0.0f;
  crackle_ = 0.0f;
  stationBlend_ = 0.0f;
  sweepCycle_ = static_cast<uint32_t>(random(0L, 2L));
  sweepPosInCycle_ = 0U;
  sampleClock_ = 0U;
  sonarPhase_ = 0.0f;
  sonarEchoPhase_ = 0.0f;
  morsePhase_ = 0.0f;
  winPhase_ = 0.0f;
  morseToneSamplesLeft_ = 0U;
  morseGapSamplesLeft_ = 0U;
  morsePatternPos_ = 0U;
  winStepSamplesLeft_ = 0U;
  winStepTotalSamples_ = 0U;
  winStepIndex_ = 0U;
  winCurrentFreqHz_ = 0U;

  if (synth_ != nullptr) {
    synth_->sweepOsc.setPhase(0U);
    synth_->stationOsc.setPhase(0U);
    synth_->carrierOsc.setPhase(0U);
  }

  if (effect_ == Effect::kMorse) {
    morsePrepareNextState();
  } else if (effect_ == Effect::kWin) {
    winPrepareNextStep();
  }
}

int16_t FmRadioScanFx::nextSample() {
  switch (effect_) {
    case Effect::kSonar:
      return nextSampleSonar();
    case Effect::kMorse:
      return nextSampleMorse();
    case Effect::kWin:
      return nextSampleWin();
    case Effect::kFmSweep:
    default:
      return nextSampleFmSweep();
  }
}

int16_t FmRadioScanFx::nextSampleFmSweep() {
  if (synth_ == nullptr) {
    return 0;
  }

  const uint32_t sweepPeriodSamples = (sampleRateHz_ * 2800UL) / 1000UL;
  float sweepT = (sweepPeriodSamples > 0U)
                     ? (static_cast<float>(sweepPosInCycle_) / static_cast<float>(sweepPeriodSamples))
                     : 0.0f;
  if ((sweepCycle_ & 1U) != 0U) {
    sweepT = 1.0f - sweepT;
  }

  const bool stationWindow = (sweepT > 0.20f && sweepT < 0.33f) || (sweepT > 0.58f && sweepT < 0.74f);
  const float stationTarget = stationWindow ? 1.0f : 0.0f;
  stationBlend_ += (stationTarget - stationBlend_) * 0.0045f;

  const float sweepHz = 85.0f + (980.0f * sweepT);
  const float driftHz = 0.08f;
  const float wowHz = 0.20f;
  const float stationHz = 165.0f + (125.0f * sinf(sweepLfoPhase_));
  const float carrierHz = stationWindow
                              ? (stationHz * (2.25f + (0.18f * sinf(driftLfoPhase_))))
                              : (420.0f + (170.0f * sinf(driftLfoPhase_)));

  const float rateCorrection = static_cast<float>(kSynthRateHz) / static_cast<float>(sampleRateHz_);
  synth_->sweepOsc.setFreq(sweepHz * rateCorrection);
  synth_->stationOsc.setFreq(stationHz * rateCorrection);
  synth_->carrierOsc.setFreq(carrierHz * rateCorrection);

  const float sweepWave = static_cast<float>(synth_->sweepOsc.next()) / 128.0f;
  const float stationWave = static_cast<float>(synth_->stationOsc.next()) / 128.0f;
  const float carrierWave = static_cast<float>(synth_->carrierOsc.next()) / 128.0f;

  const float noiseRaw = static_cast<float>(random(-128L, 128L)) / 128.0f;
  noiseLp_ = (0.985f * noiseLp_) + (0.015f * noiseRaw);
  const float hiss = noiseRaw - noiseLp_;

  if (random(0L, 1000L) < 4L) {
    crackle_ = static_cast<float>(random(-128L, 128L)) / 128.0f;
  }
  const float crackle = crackle_;
  crackle_ *= stationWindow ? 0.78f : 0.90f;

  const float t = static_cast<float>(sampleClock_) / static_cast<float>(sampleRateHz_);
  const float seekFlutter = 0.83f + (0.17f * sinf(kTwoPi * 0.45f * t));
  const bool softDropout = !stationWindow && (((sampleClock_ / ((sampleRateHz_ / 11U) + 1U)) % 19U) == 7U);
  const float dropoutGain = softDropout ? 0.34f : 1.0f;

  float sampleF = 0.0f;
  sampleF += 0.40f * sweepWave;
  sampleF += 0.22f * carrierWave;
  sampleF += stationBlend_ * (0.24f * stationWave + 0.14f * sweepWave * carrierWave);
  sampleF += (0.56f - (0.37f * stationBlend_)) * hiss;
  sampleF += 0.20f * crackle;
  sampleF *= seekFlutter * dropoutGain;
  sampleF *= gain_;
  sampleF = clampf(sampleF, -1.0f, 1.0f);

  sweepLfoPhase_ += kTwoPi * (wowHz / static_cast<float>(sampleRateHz_));
  if (sweepLfoPhase_ >= kTwoPi) {
    sweepLfoPhase_ -= kTwoPi;
  }
  driftLfoPhase_ += kTwoPi * (driftHz / static_cast<float>(sampleRateHz_));
  if (driftLfoPhase_ >= kTwoPi) {
    driftLfoPhase_ -= kTwoPi;
  }

  ++sampleClock_;
  ++sweepPosInCycle_;
  if (sweepPeriodSamples > 0U && sweepPosInCycle_ >= sweepPeriodSamples) {
    sweepPosInCycle_ = 0U;
    ++sweepCycle_;
  }

  return static_cast<int16_t>(sampleF * 32000.0f);
}

int16_t FmRadioScanFx::nextSampleSonar() {
  const uint32_t periodSamples = (sampleRateHz_ * 1300UL) / 1000UL;
  const uint32_t pingSamples = (sampleRateHz_ * 150UL) / 1000UL;
  const uint32_t echoStartSamples = (sampleRateHz_ * 220UL) / 1000UL;
  const uint32_t echoLenSamples = (sampleRateHz_ * 540UL) / 1000UL;

  const uint32_t cycle = (periodSamples > 0U) ? (sampleClock_ % periodSamples) : 0U;
  float sampleF = 0.0f;

  if (cycle < pingSamples && pingSamples > 0U) {
    const float pingT = static_cast<float>(cycle) / static_cast<float>(pingSamples);
    const float freqHz = 1800.0f - (1200.0f * pingT);
    sonarPhase_ += kTwoPi * (freqHz / static_cast<float>(sampleRateHz_));
    if (sonarPhase_ >= kTwoPi) {
      sonarPhase_ -= kTwoPi;
    }
    const float env = (1.0f - pingT) * (1.0f - pingT);
    sampleF += 0.90f * sinf(sonarPhase_) * env;
    if (cycle < ((sampleRateHz_ * 4UL) / 1000UL)) {
      sampleF += 0.22f;
    }
  }

  if (cycle >= echoStartSamples && cycle < (echoStartSamples + echoLenSamples) && echoLenSamples > 0U) {
    const uint32_t echoPos = cycle - echoStartSamples;
    const float echoT = static_cast<float>(echoPos) / static_cast<float>(echoLenSamples);
    const float freqHz = 760.0f - (240.0f * echoT);
    sonarEchoPhase_ += kTwoPi * (freqHz / static_cast<float>(sampleRateHz_));
    if (sonarEchoPhase_ >= kTwoPi) {
      sonarEchoPhase_ -= kTwoPi;
    }
    const float env = expf(-4.5f * echoT);
    sampleF += 0.46f * sinf(sonarEchoPhase_) * env;
  }

  sampleF += 0.03f * (static_cast<float>(random(-128L, 128L)) / 128.0f);
  sampleF *= gain_;
  sampleF = clampf(sampleF, -1.0f, 1.0f);

  ++sampleClock_;
  return static_cast<int16_t>(sampleF * 32000.0f);
}

bool FmRadioScanFx::morsePrepareNextState() {
  const uint32_t unitSamplesRaw = (sampleRateHz_ * static_cast<uint32_t>(kMorseUnitMs)) / 1000UL;
  const uint32_t unitSamples = (unitSamplesRaw > 0U) ? unitSamplesRaw : 1U;

  while (true) {
    const char symbol = kMorsePattern[morsePatternPos_];
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

int16_t FmRadioScanFx::nextSampleMorse() {
  if (morseToneSamplesLeft_ == 0U) {
    if (morseGapSamplesLeft_ > 0U) {
      --morseGapSamplesLeft_;
      ++sampleClock_;
      return 0;
    }
    if (!morsePrepareNextState()) {
      ++sampleClock_;
      return 0;
    }
  }

  const float warble = 1.0f + (0.05f * sinf(kTwoPi * 0.7f *
                                             (static_cast<float>(sampleClock_) /
                                              static_cast<float>(sampleRateHz_))));
  const float freqHz = static_cast<float>(kMorseFreqHz) * warble;
  morsePhase_ += kTwoPi * (freqHz / static_cast<float>(sampleRateHz_));
  if (morsePhase_ >= kTwoPi) {
    morsePhase_ -= kTwoPi;
  }

  float sampleF = 0.82f * sinf(morsePhase_);
  sampleF += 0.10f * sinf(morsePhase_ * 2.0f);
  sampleF *= gain_;
  sampleF = clampf(sampleF, -1.0f, 1.0f);

  --morseToneSamplesLeft_;
  ++sampleClock_;
  return static_cast<int16_t>(sampleF * 32000.0f);
}

bool FmRadioScanFx::winPrepareNextStep() {
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

int16_t FmRadioScanFx::nextSampleWin() {
  if (winStepSamplesLeft_ == 0U && !winPrepareNextStep()) {
    ++sampleClock_;
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
    float env = 1.0f - (0.72f * progress);

    const uint32_t attackSamplesRaw = (sampleRateHz_ * 4UL) / 1000UL;
    const uint32_t releaseSamplesRaw = (sampleRateHz_ * 16UL) / 1000UL;
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

    sampleF = (0.72f * sineWave) + (0.28f * squareWave);
    sampleF += 0.18f * sinf(winPhase_ * 1.5f);
    sampleF *= env;
  }

  if (winStepSamplesLeft_ > 0U) {
    --winStepSamplesLeft_;
  }

  sampleF *= gain_;
  sampleF = clampf(sampleF, -1.0f, 1.0f);

  ++sampleClock_;
  return static_cast<int16_t>(sampleF * 32000.0f);
}
