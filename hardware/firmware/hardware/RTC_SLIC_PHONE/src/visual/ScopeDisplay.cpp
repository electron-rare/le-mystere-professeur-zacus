#include "ScopeDisplay.h"

#include <math.h>
#if defined(CONFIG_IDF_TARGET_ESP32)
#include <driver/dac.h>
#endif

namespace {
constexpr uint8_t kDefaultAmplitude = 48U;
constexpr uint16_t kDefaultFrequencyHz = 1200U;
constexpr uint16_t kMinFrequencyHz = 60U;
constexpr uint16_t kMaxFrequencyHz = 5000U;
}  // namespace

ScopeDisplay::ScopeDisplay()
    : initialized_(false),
      configured_(false),
      enabled_(false),
      supported_(false),
      frequency_hz_(kDefaultFrequencyHz),
      amplitude_(kDefaultAmplitude),
      last_tick_us_(0),
      phase_(0.0f) {}

bool ScopeDisplay::supported() const {
    return supported_;
}

bool ScopeDisplay::enabled() const {
    return initialized_ && enabled_;
}

uint16_t ScopeDisplay::frequency() const {
    return frequency_hz_;
}

uint8_t ScopeDisplay::amplitude() const {
    return amplitude_;
}

bool ScopeDisplay::begin() {
#if defined(CONFIG_IDF_TARGET_ESP32)
    dac_output_enable(DAC_CHANNEL_1);
    dac_output_enable(DAC_CHANNEL_2);
    initialized_ = true;
    supported_ = true;
    configured_ = true;
    enabled_ = true;
    last_tick_us_ = micros();
    phase_ = 0.0f;
    return true;
#else
    initialized_ = false;
    supported_ = false;
    configured_ = false;
    enabled_ = false;
    return false;
#endif
}

void ScopeDisplay::end() {
    if (!initialized_) {
        return;
    }

    enabled_ = false;
    #if defined(CONFIG_IDF_TARGET_ESP32)
    dac_output_disable(DAC_CHANNEL_1);
    dac_output_disable(DAC_CHANNEL_2);
    #endif
    initialized_ = false;
}

bool ScopeDisplay::configure(uint16_t frequency_hz, uint8_t amplitude) {
    if (frequency_hz < kMinFrequencyHz || frequency_hz > kMaxFrequencyHz) {
        return false;
    }
    if (amplitude == 0U) {
        return false;
    }
    frequency_hz_ = frequency_hz;
    amplitude_ = amplitude;
    configured_ = true;
    return true;
}

void ScopeDisplay::enable(bool value) {
    if (!configured_ || !supported_) {
        return;
    }
    enabled_ = value;
    if (enabled_) {
        if (!initialized_) {
            begin();
        }
    }
}

void ScopeDisplay::tick() {
    if (!initialized_ || !enabled_ || !configured_) {
        return;
    }

#if defined(CONFIG_IDF_TARGET_ESP32)
    const uint32_t now = micros();
    if ((now - last_tick_us_) < kTickIntervalUs) {
        return;
    }
    last_tick_us_ = now;

    const float step = kTau * static_cast<float>(frequency_hz_) * (kTickIntervalUs / 1000000.0f);
    phase_ += step;
    if (phase_ >= kTau) {
        phase_ -= kTau;
    }

    const float x = sinf(phase_);
    const float y = cosf(phase_);
    const int v1 = 128 + static_cast<int>(x * static_cast<float>(amplitude_));
    const int v2 = 128 + static_cast<int>(y * static_cast<float>(amplitude_));
    const uint8_t sample1 = static_cast<uint8_t>(constrain(v1, 0, 255));
    const uint8_t sample2 = static_cast<uint8_t>(constrain(v2, 0, 255));
    dac_output_voltage(DAC_CHANNEL_1, sample1);
    dac_output_voltage(DAC_CHANNEL_2, sample2);
#endif
}
