// hardware_manager.cpp - Freenove peripherals (WS2812, mic, battery, buttons).
#include "hardware_manager.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "ui_freenove_config.h"

namespace {

constexpr uint8_t kDefaultLedBrightness = static_cast<uint8_t>(FREENOVE_WS2812_BRIGHTNESS);
constexpr float kTwoPi = 6.2831853f;
constexpr float kTunerReferenceHz = 440.0f;
constexpr uint16_t kTunerMinHz = 80U;
constexpr uint16_t kTunerMaxHz = 1200U;
constexpr uint16_t kLaDetectMinHz = 320U;
constexpr uint16_t kLaDetectMaxHz = 560U;
constexpr uint8_t kTunerMinConfidence = 18U;
constexpr uint8_t kTunerDisplayMinConfidence = 40U;
constexpr uint8_t kTunerDisplayMinLevelPct = 10U;
constexpr uint16_t kTunerDisplayMinPeak = 1000U;
constexpr uint16_t kMicAgcTargetPeak = 7600U;
constexpr uint16_t kMicAgcDefaultGainQ8 = 256U;
constexpr uint16_t kMicAgcMinGainQ8 = 192U;
constexpr uint16_t kMicAgcMaxGainQ8 = 1024U;
constexpr uint16_t kMicAgcActivePeakMin = 28U;
constexpr uint16_t kMicAgcSignalDisplayPeakMin = 170U;
constexpr uint16_t kMicAgcStrongSignalPeakMin = 640U;
constexpr uint16_t kMicAgcWeakSignalReleaseMs = 450U;
constexpr uint16_t kMicAgcMinLevelDen = 5600U;
constexpr uint16_t kMicAgcAmbientGateDiv = 10U;
constexpr uint16_t kMicAgcGainDeadbandQ8 = 18U;
constexpr uint16_t kMicAgcMaxGainStepUp = 48U;
constexpr uint16_t kMicAgcMaxGainStepDown = 16U;
uint8_t clampU8(int value) {
  if (value < 0) {
    return 0U;
  }
  if (value > 255) {
    return 255U;
  }
  return static_cast<uint8_t>(value);
}

uint32_t hash32(uint32_t value) {
  value ^= value >> 16;
  value *= 0x7feb352dUL;
  value ^= value >> 15;
  value *= 0x846ca68bUL;
  value ^= value >> 16;
  return value;
}

uint8_t computeLevelPercent(uint16_t effective_peak, uint16_t den) {
  const uint8_t raw_level =
      static_cast<uint8_t>(std::min<uint32_t>(100U, (static_cast<uint32_t>(effective_peak) * 100U) / den));
  return raw_level;
}

}  // namespace

HardwareManager::HardwareManager()
    : strip_(FREENOVE_WS2812_COUNT, FREENOVE_WS2812_PIN, NEO_GRB + NEO_KHZ800) {
  snapshot_.led_brightness = kDefaultLedBrightness;
  snapshot_.mic_gain_percent = static_cast<uint16_t>((mic_agc_gain_q8_ * 100U) / 256U);
  snapshot_.mic_noise_floor = mic_noise_floor_raw_;
  setScenePalette("SCENE_READY");
}

bool HardwareManager::begin() {
  snapshot_.ready = true;

#if FREENOVE_WS2812_PIN >= 0 && FREENOVE_WS2812_COUNT > 0
  strip_.begin();
  strip_.setBrightness(snapshot_.led_brightness);
  strip_.clear();
  strip_.show();
  snapshot_.ws2812_ready = true;
  Serial.printf("[HW] WS2812 ready pin=%d count=%d\n", FREENOVE_WS2812_PIN, FREENOVE_WS2812_COUNT);
#else
  snapshot_.ws2812_ready = false;
#endif

#if FREENOVE_BAT_ADC_PIN >= 0
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);
  pinMode(FREENOVE_BAT_ADC_PIN, INPUT);
  snapshot_.battery_ready = true;
  Serial.printf("[HW] battery ADC ready pin=%d\n", FREENOVE_BAT_ADC_PIN);
#else
  snapshot_.battery_ready = false;
#endif

#if FREENOVE_BAT_CHARGE_PIN >= 0
  pinMode(FREENOVE_BAT_CHARGE_PIN, INPUT_PULLUP);
#endif

  snapshot_.mic_ready = beginMic();
  if (snapshot_.mic_ready) {
    Serial.printf("[HW] mic I2S ready sck=%d ws=%d din=%d\n", FREENOVE_I2S_IN_SCK, FREENOVE_I2S_IN_WS, FREENOVE_I2S_IN_DIN);
  } else {
    Serial.println("[HW] mic I2S unavailable");
  }

  next_led_ms_ = 0U;
  next_mic_ms_ = 0U;
  next_battery_ms_ = 0U;
  update(0U);
  return true;
}

void HardwareManager::update(uint32_t now_ms) {
  updateMic(now_ms);
  updateBattery(now_ms);
  updateLed(now_ms);
}

void HardwareManager::noteButton(uint8_t key, bool long_press, uint32_t now_ms) {
  snapshot_.last_button = key;
  snapshot_.last_button_long = long_press;
  snapshot_.last_button_ms = now_ms;
  ++snapshot_.button_count;
  button_flash_until_ms_ = now_ms + kButtonFlashMs;
}

void HardwareManager::setSceneHint(const char* scene_id) {
  if (scene_id == nullptr || scene_id[0] == '\0') {
    return;
  }
  if (std::strncmp(snapshot_.scene_id, scene_id, sizeof(snapshot_.scene_id) - 1U) == 0) {
    return;
  }
  setScenePalette(scene_id);
}

bool HardwareManager::setManualLed(uint8_t r, uint8_t g, uint8_t b, uint8_t brightness, bool pulse) {
  manual_led_ = true;
  manual_pulse_ = pulse;
  manual_r_ = r;
  manual_g_ = g;
  manual_b_ = b;
  manual_brightness_ = brightness;
  snapshot_.led_manual = true;
  next_led_ms_ = 0U;
  return snapshot_.ws2812_ready;
}

void HardwareManager::clearManualLed() {
  manual_led_ = false;
  manual_pulse_ = false;
  snapshot_.led_manual = false;
  next_led_ms_ = 0U;
}

HardwareManager::Snapshot HardwareManager::snapshot() const {
  return snapshot_;
}

bool HardwareManager::beginMic() {
  i2s_config_t config = {};
  config.mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_RX);
  config.sample_rate = kMicSampleRate;
  config.bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT;
  config.channel_format = I2S_CHANNEL_FMT_ONLY_LEFT;
  config.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  config.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
  config.dma_buf_count = 4;
  config.dma_buf_len = 128;
  config.use_apll = false;
  config.tx_desc_auto_clear = false;
  config.fixed_mclk = 0;

  if (i2s_driver_install(kMicPort, &config, 0, nullptr) != ESP_OK) {
    return false;
  }

  i2s_pin_config_t pin_config = {};
  pin_config.bck_io_num = FREENOVE_I2S_IN_SCK;
  pin_config.ws_io_num = FREENOVE_I2S_IN_WS;
  pin_config.data_out_num = I2S_PIN_NO_CHANGE;
  pin_config.data_in_num = FREENOVE_I2S_IN_DIN;
  if (i2s_set_pin(kMicPort, &pin_config) != ESP_OK) {
    i2s_driver_uninstall(kMicPort);
    return false;
  }

  if (i2s_set_clk(kMicPort, kMicSampleRate, I2S_BITS_PER_SAMPLE_32BIT, I2S_CHANNEL_MONO) != ESP_OK) {
    i2s_driver_uninstall(kMicPort);
    return false;
  }

  mic_driver_ready_ = true;
  return true;
}

void HardwareManager::updateMic(uint32_t now_ms) {
  if (!snapshot_.mic_ready) {
    return;
  }
  if (now_ms < next_mic_ms_) {
    return;
  }
  next_mic_ms_ = now_ms + kMicPeriodMs;

  size_t bytes_read = 0U;
  if (i2s_read(kMicPort, mic_raw_samples_, sizeof(mic_raw_samples_), &bytes_read, 0) != ESP_OK || bytes_read == 0U) {
    return;
  }

  const size_t sample_count = bytes_read / sizeof(int32_t);
  if (sample_count == 0U) {
    return;
  }
  uint16_t raw_peak = 0U;
  uint32_t raw_abs_sum = 0U;
  for (size_t index = 0U; index < sample_count; ++index) {
    // INMP441 data arrives as signed PCM24 packed in 32-bit slots (left-aligned).
    int32_t value = mic_raw_samples_[index] >> 16;
    if (value > 32767) {
      value = 32767;
    } else if (value < -32768) {
      value = -32768;
    }
    const uint16_t abs_raw = static_cast<uint16_t>((value < 0) ? -value : value);
    if (abs_raw > raw_peak) {
      raw_peak = abs_raw;
    }
    raw_abs_sum += static_cast<uint32_t>(abs_raw);

    // Apply dynamic digital gain before pitch/level extraction.
    int32_t scaled = (value * static_cast<int32_t>(mic_agc_gain_q8_)) / 256;
    if (scaled > 32767) {
      scaled = 32767;
    } else if (scaled < -32768) {
      scaled = -32768;
    }
    mic_samples_[index] = static_cast<int16_t>(scaled);
  }

  const uint16_t raw_abs_mean = static_cast<uint16_t>(
      std::min<uint32_t>(65535U, raw_abs_sum / static_cast<uint32_t>(sample_count)));

  // Track ambient floor from raw microphone average levels to avoid over-amplifying idle noise.
  if (raw_abs_mean <= static_cast<uint16_t>(mic_noise_floor_raw_ + 24U)) {
    mic_noise_floor_raw_ =
        static_cast<uint16_t>((static_cast<uint32_t>(mic_noise_floor_raw_) * 31U + raw_abs_mean) / 32U);
  } else {
    mic_noise_floor_raw_ =
        static_cast<uint16_t>((static_cast<uint32_t>(mic_noise_floor_raw_) * 127U + raw_abs_mean) / 128U);
  }
  if (mic_noise_floor_raw_ < 24U) {
    mic_noise_floor_raw_ = 24U;
  }

  const uint16_t signal_abs_raw = (raw_abs_mean > mic_noise_floor_raw_)
                                     ? static_cast<uint16_t>(raw_abs_mean - mic_noise_floor_raw_)
                                     : 0U;
  const uint16_t dynamic_active_peak_min =
      std::max<uint16_t>(kMicAgcActivePeakMin, static_cast<uint16_t>(mic_noise_floor_raw_ / kMicAgcAmbientGateDiv));
  const bool has_signal_window = signal_abs_raw >= dynamic_active_peak_min;
  const bool has_stale_signal = (now_ms - mic_last_signal_ms_) > static_cast<uint32_t>(kMicAgcWeakSignalReleaseMs);
  if (has_signal_window) {
    mic_last_signal_ms_ = now_ms;
  }

  uint16_t target_gain_q8 = mic_agc_gain_q8_;
  if (has_signal_window) {
    const uint32_t desired = (static_cast<uint32_t>(kMicAgcTargetPeak) * 256U) /
                            static_cast<uint32_t>(std::max<uint16_t>(signal_abs_raw, 1U));
    target_gain_q8 = static_cast<uint16_t>(
        std::min<uint32_t>(kMicAgcMaxGainQ8, std::max<uint32_t>(kMicAgcMinGainQ8, desired)));
  } else if (raw_abs_mean <= static_cast<uint16_t>(mic_noise_floor_raw_ + 24U) || has_stale_signal) {
    target_gain_q8 = kMicAgcDefaultGainQ8;
  }

  const bool gain_return_from_silence = !has_signal_window &&
                                        (raw_abs_mean <= static_cast<uint16_t>(mic_noise_floor_raw_ + 24U) || has_stale_signal);

  if ((target_gain_q8 > (mic_agc_gain_q8_ + kMicAgcGainDeadbandQ8))) {
    uint16_t delta = static_cast<uint16_t>(target_gain_q8 - mic_agc_gain_q8_);
    uint16_t step = static_cast<uint16_t>((delta / 10U) + 3U);
    if (step < 6U) {
      step = 6U;
    }
    if (gain_return_from_silence) {
      step = static_cast<uint16_t>(std::max<uint16_t>(8U, (delta / 12U) + 2U));
      if (step > kMicAgcMaxGainStepUp) {
        step = kMicAgcMaxGainStepUp;
      }
    } else if (signal_abs_raw < kMicAgcStrongSignalPeakMin) {
      step = static_cast<uint16_t>((step < 10U) ? 10U : step);
    }
    if (step > kMicAgcMaxGainStepUp) {
      step = kMicAgcMaxGainStepUp;
    }
    mic_agc_gain_q8_ = static_cast<uint16_t>(mic_agc_gain_q8_ + step);
  } else if ((mic_agc_gain_q8_ > (target_gain_q8 + kMicAgcGainDeadbandQ8))) {
    uint16_t delta = static_cast<uint16_t>(mic_agc_gain_q8_ - target_gain_q8);
    uint16_t step = static_cast<uint16_t>((delta / 10U) + 4U);
    if (step < 8U) {
      step = 8U;
    }
    if (signal_abs_raw > kMicAgcStrongSignalPeakMin) {
      step = static_cast<uint16_t>((step < 24U) ? 24U : step);
    }
    if (gain_return_from_silence) {
      step = static_cast<uint16_t>(std::max<uint16_t>(12U, (delta / 6U) + 4U));
      if (step > kMicAgcMaxGainStepDown) {
        step = kMicAgcMaxGainStepDown;
      }
    }
    if (step > kMicAgcMaxGainStepDown) {
      step = kMicAgcMaxGainStepDown;
    }
    mic_agc_gain_q8_ = static_cast<uint16_t>(mic_agc_gain_q8_ - step);
  }
  if (mic_agc_gain_q8_ < kMicAgcMinGainQ8) {
    mic_agc_gain_q8_ = kMicAgcMinGainQ8;
  } else if (mic_agc_gain_q8_ > kMicAgcMaxGainQ8) {
    mic_agc_gain_q8_ = kMicAgcMaxGainQ8;
  }

  uint16_t peak = 0U;
  for (size_t index = 0U; index < sample_count; ++index) {
    int value = static_cast<int>(mic_samples_[index]);
    if (value < 0) {
      value = -value;
    }
    if (value > peak) {
      peak = static_cast<uint16_t>(value);
    }
  }

  const uint16_t noise_floor_scaled = static_cast<uint16_t>(
      std::min<uint32_t>(4095U, (static_cast<uint32_t>(mic_noise_floor_raw_) * mic_agc_gain_q8_) / 256U));
  const uint16_t effective_peak = (peak > noise_floor_scaled) ? static_cast<uint16_t>(peak - noise_floor_scaled) : 0U;
  snapshot_.mic_peak = peak;
  snapshot_.mic_noise_floor = mic_noise_floor_raw_;
  snapshot_.mic_gain_percent = static_cast<uint16_t>((static_cast<uint32_t>(mic_agc_gain_q8_) * 100U) / 256U);

  uint16_t freq_hz = 0U;
  int16_t cents = 0;
  uint8_t confidence = 0U;
  estimatePitchFromSamples(mic_samples_,
                          sample_count,
                          freq_hz,
                          cents,
                          confidence);
  const bool has_pitch = (confidence > 0U) && (freq_hz > 0U);

  if (has_pitch) {
    snapshot_.mic_freq_hz = freq_hz;
    snapshot_.mic_pitch_cents = cents;
    snapshot_.mic_pitch_confidence = confidence;
  } else {
    snapshot_.mic_freq_hz = 0U;
    snapshot_.mic_pitch_cents = 0;
    snapshot_.mic_pitch_confidence = 0U;
  }

  const uint16_t level_for_display = computeLevelPercent(effective_peak, kMicAgcMinLevelDen);
  const uint16_t level_for_waveform = (effective_peak >= kMicAgcSignalDisplayPeakMin) ? level_for_display : 0U;
  uint8_t level = 0U;
  if (level_for_waveform > 0U) {
    level = static_cast<uint8_t>(std::min<uint16_t>(100U, (static_cast<uint16_t>(snapshot_.mic_level_percent) * 3U + level_for_waveform) / 4U));
  }
  snapshot_.mic_level_percent = level;
  snapshot_.mic_waveform[snapshot_.mic_waveform_head] = level;
  snapshot_.mic_waveform_head = static_cast<uint8_t>((snapshot_.mic_waveform_head + 1U) % kMicWaveformCapacity);
  if (snapshot_.mic_waveform_count < kMicWaveformCapacity) {
    ++snapshot_.mic_waveform_count;
  }
}

void HardwareManager::updateBattery(uint32_t now_ms) {
  if (!snapshot_.battery_ready) {
    return;
  }
  if (now_ms < next_battery_ms_) {
    return;
  }
  next_battery_ms_ = now_ms + kBatteryPeriodMs;

  uint32_t total_mv = 0U;
  uint8_t valid = 0U;
  for (uint8_t index = 0U; index < 10U; ++index) {
    const int mv = analogReadMilliVolts(FREENOVE_BAT_ADC_PIN);
    if (mv <= 0) {
      continue;
    }
    total_mv += static_cast<uint32_t>(mv);
    ++valid;
    delayMicroseconds(120);
  }
  if (valid == 0U) {
    return;
  }

  const float adc_mv = static_cast<float>(total_mv) / static_cast<float>(valid);
  float cell_mv = adc_mv * FREENOVE_BAT_VOLT_MULTIPLIER + FREENOVE_BAT_VOLT_OFFSET_MV;
  if (cell_mv < 0.0f) {
    cell_mv = 0.0f;
  }

  snapshot_.battery_mv = static_cast<uint16_t>(adc_mv);
  snapshot_.battery_cell_mv = static_cast<uint16_t>(cell_mv);
  snapshot_.battery_percent = batteryPercentFromMv(snapshot_.battery_cell_mv);
#if FREENOVE_BAT_CHARGE_PIN >= 0
  snapshot_.charging = (digitalRead(FREENOVE_BAT_CHARGE_PIN) == LOW);
#else
  snapshot_.charging = false;
#endif
}

void HardwareManager::updateLed(uint32_t now_ms) {
  if (!snapshot_.ws2812_ready) {
    return;
  }
  if (now_ms < next_led_ms_) {
    return;
  }
  next_led_ms_ = now_ms + kLedPeriodMs;

  uint8_t base_r = scene_r_;
  uint8_t base_g = scene_g_;
  uint8_t base_b = scene_b_;
  uint8_t brightness = scene_brightness_;
  bool pulse = led_pulse_;

  if (manual_led_) {
    base_r = manual_r_;
    base_g = manual_g_;
    base_b = manual_b_;
    brightness = manual_brightness_;
    pulse = manual_pulse_;
  }
  if (button_flash_until_ms_ > now_ms) {
    base_r = 255U;
    base_g = 220U;
    base_b = 120U;
    brightness = 90U;
    pulse = false;
  }

  if (!manual_led_ && button_flash_until_ms_ <= now_ms && isTunerSceneHint()) {
    applyTunerLedPattern(now_ms, base_r, base_g, base_b, brightness);
    return;
  }

  if (!manual_led_ && button_flash_until_ms_ <= now_ms && isBrokenSceneHint()) {
    applyBrokenLedPattern(now_ms, base_r, base_g, base_b, brightness);
    return;
  }

  float dim = 1.0f;
  if (pulse) {
    const float phase = static_cast<float>(now_ms % 1400U) / 1400.0f;
    dim = 0.30f + (0.70f * (0.5f + 0.5f * std::sin(phase * kTwoPi)));
  }
  const uint8_t out_r = clampU8(static_cast<int>(static_cast<float>(base_r) * dim));
  const uint8_t out_g = clampU8(static_cast<int>(static_cast<float>(base_g) * dim));
  const uint8_t out_b = clampU8(static_cast<int>(static_cast<float>(base_b) * dim));

  strip_.setBrightness(clampU8(brightness));
  for (uint16_t index = 0U; index < FREENOVE_WS2812_COUNT; ++index) {
    strip_.setPixelColor(index, out_r, out_g, out_b);
  }
  strip_.show();

  snapshot_.led_r = out_r;
  snapshot_.led_g = out_g;
  snapshot_.led_b = out_b;
  snapshot_.led_brightness = brightness;
}

bool HardwareManager::isBrokenSceneHint() const {
  return (std::strcmp(snapshot_.scene_id, "SCENE_LOCKED") == 0) ||
         (std::strcmp(snapshot_.scene_id, "SCENE_BROKEN") == 0) ||
         (std::strcmp(snapshot_.scene_id, "SCENE_SIGNAL_SPIKE") == 0);
}

bool HardwareManager::isTunerSceneHint() const {
  return (std::strcmp(snapshot_.scene_id, "SCENE_LA_DETECT") == 0) ||
         (std::strcmp(snapshot_.scene_id, "SCENE_LA_DETECTOR") == 0) ||
         (std::strcmp(snapshot_.scene_id, "SCENE_SEARCH") == 0);
}

void HardwareManager::applyBrokenLedPattern(uint32_t now_ms,
                                            uint8_t base_r,
                                            uint8_t base_g,
                                            uint8_t base_b,
                                            uint8_t brightness) {
  const uint16_t led_count = FREENOVE_WS2812_COUNT;
  if (led_count == 0U) {
    return;
  }

  uint8_t effective_brightness = brightness;
  if (effective_brightness < 92U) {
    effective_brightness = 92U;
  }
  if (effective_brightness > 148U) {
    effective_brightness = 148U;
  }
  strip_.setBrightness(clampU8(effective_brightness));

  uint8_t first_r = 0U;
  uint8_t first_g = 0U;
  uint8_t first_b = 0U;

  const uint32_t slot = now_ms / 46U;
  const uint32_t in_slot = now_ms % 46U;
  const uint32_t slot_noise = hash32(slot * 2654435761UL + 0x9e3779b9UL);
  const uint16_t primary_led = static_cast<uint16_t>(slot_noise % led_count);
  const uint8_t primary_window_ms = static_cast<uint8_t>(7U + ((slot_noise >> 16) % 11U));
  const bool primary_active = in_slot < primary_window_ms;

  uint16_t secondary_led = primary_led;
  bool secondary_active = false;
  if (led_count > 1U) {
    secondary_led = static_cast<uint16_t>((primary_led + 1U + ((slot_noise >> 8) % (led_count - 1U))) % led_count);
    secondary_active = (((slot_noise >> 27) & 0x1U) == 1U) && (in_slot >= 24U) && (in_slot < 29U);
  }

  for (uint16_t index = 0U; index < led_count; ++index) {
    const uint32_t led_noise = hash32(slot_noise ^ (static_cast<uint32_t>(index + 1U) * 0x27d4eb2dUL));
    int out_r = 0;
    int out_g = 0;
    int out_b = 0;

    if (primary_active && index == primary_led) {
      const float attack = 1.0f - (static_cast<float>(in_slot) / static_cast<float>(primary_window_ms));
      const float dim = 0.88f + 0.55f * attack;
      out_r = static_cast<int>(static_cast<float>(base_r) * dim) + static_cast<int>((led_noise >> 0) & 0x2fU);
      out_g = static_cast<int>(static_cast<float>(base_g) * (0.30f + 0.95f * attack)) +
              static_cast<int>((led_noise >> 8) & 0x17U);
      out_b = static_cast<int>(static_cast<float>(base_b) * (0.18f + 0.85f * attack)) +
              static_cast<int>((led_noise >> 16) & 0x3fU);
    } else if (secondary_active && index == secondary_led) {
      out_r = static_cast<int>(base_r * 0.45f) + static_cast<int>((led_noise >> 8) & 0x1fU);
      out_g = static_cast<int>(base_g * 0.28f) + static_cast<int>((led_noise >> 16) & 0x0fU);
      out_b = static_cast<int>(base_b * 0.40f) + static_cast<int>((led_noise >> 24) & 0x2fU);
    } else {
      const bool ghost = (((led_noise + slot + index * 5U) % 23U) == 0U) && (in_slot < 3U);
      if (ghost) {
        out_r = static_cast<int>(base_r * 0.12f);
        out_g = static_cast<int>(base_g * 0.08f);
        out_b = static_cast<int>(base_b * 0.20f) + 26;
      }
    }

    const uint8_t final_r = clampU8(out_r);
    const uint8_t final_g = clampU8(out_g);
    const uint8_t final_b = clampU8(out_b);
    strip_.setPixelColor(index, final_r, final_g, final_b);

    if (index == 0U) {
      first_r = final_r;
      first_g = final_g;
      first_b = final_b;
    }
  }

  strip_.show();
  snapshot_.led_r = first_r;
  snapshot_.led_g = first_g;
  snapshot_.led_b = first_b;
  snapshot_.led_brightness = effective_brightness;
}

void HardwareManager::applyTunerLedPattern(uint32_t now_ms,
                                          uint8_t base_r,
                                          uint8_t base_g,
                                          uint8_t base_b,
                                          uint8_t brightness) {
  (void)base_r;
  (void)base_g;
  (void)base_b;

  const uint16_t led_count = FREENOVE_WS2812_COUNT;
  if (led_count == 0U) {
    return;
  }

  uint8_t first_r = 0U;
  uint8_t first_g = 0U;
  uint8_t first_b = 0U;

  uint8_t tuned_brightness = brightness;
  if (tuned_brightness < 56U) {
    tuned_brightness = 56U;
  } else if (tuned_brightness > 136U) {
    tuned_brightness = 136U;
  }
  strip_.setBrightness(tuned_brightness);

  auto setLedScaled = [&](uint16_t index, uint8_t red, uint8_t green, uint8_t blue, float scale) {
    if (index >= led_count || scale <= 0.01f) {
      return;
    }
    if (scale > 1.0f) {
      scale = 1.0f;
    }
    const uint8_t out_r = clampU8(static_cast<int>(static_cast<float>(red) * scale));
    const uint8_t out_g = clampU8(static_cast<int>(static_cast<float>(green) * scale));
    const uint8_t out_b = clampU8(static_cast<int>(static_cast<float>(blue) * scale));
    strip_.setPixelColor(index, out_r, out_g, out_b);
    if (index == 0U) {
      first_r = out_r;
      first_g = out_g;
      first_b = out_b;
    }
  };

  for (uint16_t index = 0U; index < led_count; ++index) {
    strip_.setPixelColor(index, 0, 0, 0);
  }

  // No signal/noise state: keep all tuner LEDs off as requested.
  const bool has_signal =
      (snapshot_.mic_level_percent >= kTunerDisplayMinLevelPct) &&
      (snapshot_.mic_peak >= kMicAgcSignalDisplayPeakMin) &&
      ((snapshot_.mic_pitch_confidence >= (kTunerDisplayMinConfidence / 2U)) || (snapshot_.mic_freq_hz > 0U));
  if (!has_signal) {
    strip_.show();
    snapshot_.led_r = first_r;
    snapshot_.led_g = first_g;
    snapshot_.led_b = first_b;
    snapshot_.led_brightness = tuned_brightness;
    return;
  }

  const uint32_t slot = now_ms / 56U;
  const float pulse = 0.84f + 0.16f * std::sin(static_cast<float>(slot % 180U) * (kTwoPi / 180.0f));
  const float delta_hz = static_cast<float>(snapshot_.mic_freq_hz) - kTunerReferenceHz;
  const float abs_delta_hz = std::fabs(delta_hz);

  auto lerp_u8 = [](uint8_t a, uint8_t b, float t) -> uint8_t {
    if (t < 0.0f) {
      t = 0.0f;
    } else if (t > 1.0f) {
      t = 1.0f;
    }
    const float value = static_cast<float>(a) + (static_cast<float>(b) - static_cast<float>(a)) * t;
    return clampU8(static_cast<int>(value));
  };

  // Logical tuner map aligned with UI guidance text:
  // - "MONTE EN FREQUENCE" (delta < 0) drives the ascend side (near+extreme).
  // - "DESCENDS EN FREQUENCE" (delta > 0) drives the descend side (near+extreme).
  const uint16_t idx_descend_extreme = 0U;
  const uint16_t idx_ascend_extreme = led_count - 1U;
  const uint16_t idx_descend_near = (led_count >= 4U) ? 1U : idx_descend_extreme;
  const uint16_t idx_ascend_near = (led_count >= 4U) ? (led_count - 2U) : idx_ascend_extreme;
  const bool in_tune_center = (abs_delta_hz <= 1.8f);

  if (in_tune_center) {
    setLedScaled(idx_descend_near, 24U, 255U, 88U, pulse);
    setLedScaled(idx_ascend_near, 24U, 255U, 88U, pulse);
    setLedScaled(idx_descend_extreme, 255U, 64U, 0U, 0.05f);
    if (idx_ascend_extreme != idx_descend_extreme) {
      setLedScaled(idx_ascend_extreme, 255U, 64U, 0U, 0.05f);
    }
  } else {
    const float ratio = std::fmin(1.0f, abs_delta_hz / 10.0f);
    const float near_scale = 0.24f + 0.76f * std::fmin(1.0f, abs_delta_hz / 6.0f);
    const float extreme_scale = 0.14f + 0.86f * ratio;

    if (delta_hz < 0.0f) {
      const uint8_t near_r = lerp_u8(30U, 255U, ratio);
      const uint8_t near_g = lerp_u8(255U, 110U, ratio);
      const uint8_t extreme_g = lerp_u8(120U, 0U, ratio);
      setLedScaled(idx_ascend_near, near_r, near_g, 0U, near_scale);
      setLedScaled(idx_ascend_extreme, 255U, extreme_g, 0U, extreme_scale);
      setLedScaled(idx_descend_near, 24U, 255U, 88U, 0.10f);
    } else {
      const uint8_t near_r = lerp_u8(30U, 255U, ratio);
      const uint8_t near_g = lerp_u8(255U, 110U, ratio);
      const uint8_t extreme_g = lerp_u8(120U, 0U, ratio);
      setLedScaled(idx_descend_near, near_r, near_g, 0U, near_scale);
      setLedScaled(idx_descend_extreme, 255U, extreme_g, 0U, extreme_scale);
      setLedScaled(idx_ascend_near, 24U, 255U, 88U, 0.10f);
    }
  }

  if (led_count == 1U) {
    if (in_tune_center) {
      setLedScaled(0U, 24U, 255U, 88U, pulse);
    } else {
      setLedScaled(0U, 255U, 42U, 0U, 0.95f);
    }
  } else if (led_count == 2U) {
    if (in_tune_center) {
      setLedScaled(0U, 24U, 255U, 88U, pulse);
      setLedScaled(1U, 24U, 255U, 88U, pulse);
    } else if (delta_hz < 0.0f) {
      setLedScaled(0U, 255U, 42U, 0U, 0.95f);
      setLedScaled(1U, 255U, 180U, 0U, 0.55f);
    } else {
      setLedScaled(1U, 255U, 42U, 0U, 0.95f);
      setLedScaled(0U, 255U, 180U, 0U, 0.55f);
    }
  }

  strip_.show();
  snapshot_.led_r = first_r;
  snapshot_.led_g = first_g;
  snapshot_.led_b = first_b;
  snapshot_.led_brightness = tuned_brightness;
}

void HardwareManager::estimatePitch(uint16_t& freq_hz, int16_t& cents, uint8_t& confidence, uint16_t& peak_for_window) {
  freq_hz = snapshot_.mic_freq_hz;
  cents = snapshot_.mic_pitch_cents;
  confidence = snapshot_.mic_pitch_confidence;
  peak_for_window = snapshot_.mic_peak;
}

void HardwareManager::estimatePitchFromSamples(const int16_t* samples,
                                              size_t sample_count,
                                              uint16_t& out_freq,
                                              int16_t& out_cents,
                                              uint8_t& out_confidence) {
  out_freq = 0U;
  out_cents = 0;
  out_confidence = 0U;

  if (samples == nullptr || sample_count < 64U) {
    return;
  }
  if (sample_count > kMicReadSamples) {
    sample_count = kMicReadSamples;
  }

  int32_t sum = 0;
  int16_t peak_sample = 0;
  for (size_t index = 0U; index < sample_count; ++index) {
    const int16_t sample = samples[index];
    sum += sample;
    const int16_t abs_sample = static_cast<int16_t>((sample < 0) ? -sample : sample);
    if (abs_sample > peak_sample) {
      peak_sample = abs_sample;
    }
  }
  const float zero_reference = static_cast<float>(sum) / static_cast<float>(sample_count);
  const uint16_t peak_for_window = static_cast<uint16_t>(peak_sample > 0 ? peak_sample : 0);
  if (peak_for_window < 260U) {
    return;
  }

  pitch_energy_prefix_[0] = 0.0f;
  for (size_t index = 0U; index < sample_count; ++index) {
    const float value = static_cast<float>(samples[index]) - zero_reference;
    pitch_centered_[index] = value;
    pitch_energy_prefix_[index + 1U] = pitch_energy_prefix_[index] + (value * value);
  }

  const uint16_t detect_min_hz = std::max<uint16_t>(kTunerMinHz, kLaDetectMinHz);
  const uint16_t detect_max_hz = std::min<uint16_t>(kTunerMaxHz, kLaDetectMaxHz);
  const int lag_min = static_cast<int>(kMicSampleRate / detect_max_hz);
  int lag_max = static_cast<int>(kMicSampleRate / detect_min_hz);
  if (lag_max > static_cast<int>(sample_count) - 8) {
    lag_max = static_cast<int>(sample_count) - 8;
  }
  if (lag_min < 2 || lag_max <= lag_min) {
    return;
  }

  std::fill_n(pitch_corr_by_lag_, kMicReadSamples + 1U, 0.0f);
  int best_lag = 0;
  float best_corr = -1.0f;
  int second_lag = 0;
  float second_corr = -1.0f;

  for (int lag = lag_min; lag <= lag_max; ++lag) {
    const size_t count = sample_count - static_cast<size_t>(lag);
    float numerator = 0.0f;
    for (size_t index = 0U; index < count; ++index) {
      numerator += pitch_centered_[index] * pitch_centered_[index + static_cast<size_t>(lag)];
    }
    const float energy_a = pitch_energy_prefix_[count] - pitch_energy_prefix_[0];
    const float energy_b = pitch_energy_prefix_[sample_count] - pitch_energy_prefix_[static_cast<size_t>(lag)];
    if (energy_a <= 1.0f || energy_b <= 1.0f) {
      continue;
    }
    const float denom = std::sqrt(energy_a * energy_b);
    if (denom <= 1.0f) {
      continue;
    }
    const float corr = numerator / denom;
    pitch_corr_by_lag_[lag] = corr;
    if (corr > best_corr) {
      second_corr = best_corr;
      second_lag = best_lag;
      best_corr = corr;
      best_lag = lag;
    } else if (corr > second_corr) {
      second_corr = corr;
      second_lag = lag;
    }
  }

  if (best_lag <= 0 || best_corr < 0.10f) {
    return;
  }

  float refined_lag = static_cast<float>(best_lag);
  if (best_lag > lag_min && best_lag < lag_max) {
    const float y1 = pitch_corr_by_lag_[best_lag - 1];
    const float y2 = pitch_corr_by_lag_[best_lag];
    const float y3 = pitch_corr_by_lag_[best_lag + 1];
    const float denom = (y1 - (2.0f * y2) + y3);
    if (std::fabs(denom) > 0.0001f) {
      float shift = 0.5f * (y1 - y3) / denom;
      if (shift > 0.5f) {
        shift = 0.5f;
      } else if (shift < -0.5f) {
        shift = -0.5f;
      }
      refined_lag += shift;
    }
  }
  if (refined_lag <= 1.0f) {
    return;
  }

  const float raw_freq = static_cast<float>(kMicSampleRate) / refined_lag;
  if (raw_freq < static_cast<float>(kTunerMinHz) || raw_freq > static_cast<float>(kTunerMaxHz)) {
    return;
  }
  if (raw_freq < static_cast<float>(kLaDetectMinHz) || raw_freq > static_cast<float>(kLaDetectMaxHz)) {
    return;
  }

  const float corr_strength = std::max(0.0f, std::min(1.0f, best_corr));
  float separation = best_corr - second_corr;
  if (second_lag == 0 || separation < 0.0f) {
    separation = 0.0f;
  }
  const float sep_strength = std::max(0.0f, std::min(1.0f, separation * 4.5f));
  const float amp_strength = std::max(0.0f, std::min(1.0f, static_cast<float>(peak_for_window) / 24000.0f));
  const uint8_t confidence = static_cast<uint8_t>(
      std::round((corr_strength * 0.62f + sep_strength * 0.26f + amp_strength * 0.12f) * 100.0f));
  if (confidence < 8U) {
    return;
  }

  const float cents = 1200.0f * std::log2(raw_freq / kTunerReferenceHz);
  if (!std::isfinite(cents)) {
    return;
  }

  out_freq = static_cast<uint16_t>(raw_freq);
  out_cents = static_cast<int16_t>(std::round(cents));
  out_confidence = confidence;
}

void HardwareManager::setScenePalette(const char* scene_id) {
  std::strncpy(snapshot_.scene_id, scene_id, sizeof(snapshot_.scene_id) - 1U);
  snapshot_.scene_id[sizeof(snapshot_.scene_id) - 1U] = '\0';

  scene_brightness_ = kDefaultLedBrightness;
  led_pulse_ = true;

  if (std::strcmp(scene_id, "SCENE_LOCKED") == 0) {
    scene_r_ = 255U;
    scene_g_ = 96U;
    scene_b_ = 22U;
    scene_brightness_ = 88U;
  } else if (std::strcmp(scene_id, "SCENE_BROKEN") == 0 || std::strcmp(scene_id, "SCENE_SIGNAL_SPIKE") == 0) {
    scene_r_ = 255U;
    scene_g_ = 40U;
    scene_b_ = 18U;
    scene_brightness_ = 86U;
  } else if (std::strcmp(scene_id, "SCENE_LA_DETECT") == 0 || std::strcmp(scene_id, "SCENE_SEARCH") == 0) {
    scene_r_ = 32U;
    scene_g_ = 224U;
    scene_b_ = 170U;
    scene_brightness_ = 56U;
  } else if (std::strcmp(scene_id, "SCENE_WIN") == 0 || std::strcmp(scene_id, "SCENE_REWARD") == 0) {
    scene_r_ = 245U;
    scene_g_ = 205U;
    scene_b_ = 62U;
    scene_brightness_ = 80U;
  } else if (std::strcmp(scene_id, "SCENE_READY") == 0) {
    scene_r_ = 88U;
    scene_g_ = 214U;
    scene_b_ = 92U;
    scene_brightness_ = 52U;
  } else {
    scene_r_ = 50U;
    scene_g_ = 122U;
    scene_b_ = 255U;
    scene_brightness_ = 50U;
  }
}

uint8_t HardwareManager::batteryPercentFromMv(uint16_t cell_mv) const {
  const int min_mv = static_cast<int>(FREENOVE_BAT_VOLTAGE_MIN * 1000.0f);
  const int max_mv = static_cast<int>(FREENOVE_BAT_VOLTAGE_MAX * 1000.0f);
  if (cell_mv <= min_mv) {
    return 0U;
  }
  if (cell_mv >= max_mv) {
    return 100U;
  }
  return static_cast<uint8_t>((static_cast<uint32_t>(cell_mv - min_mv) * 100U) /
                              static_cast<uint32_t>(max_mv - min_mv));
}

uint8_t HardwareManager::clampColor(int value) {
  return clampU8(value);
}
