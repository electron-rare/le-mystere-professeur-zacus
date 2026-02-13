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

  if (synth_ != nullptr) {
    synth_->sweepOsc.setPhase(0U);
    synth_->stationOsc.setPhase(0U);
    synth_->carrierOsc.setPhase(0U);
  }
}

int16_t FmRadioScanFx::nextSample() {
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
  const bool softDropout =
      !stationWindow && (((sampleClock_ / ((sampleRateHz_ / 11U) + 1U)) % 19U) == 7U);
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
