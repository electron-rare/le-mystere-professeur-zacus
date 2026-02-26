// hardware_manager.cpp - Freenove peripherals (WS2812, mic, battery, buttons).
#include "hardware_manager.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "resources/screen_scene_registry.h"
#include "ui_freenove_config.h"

namespace {

constexpr uint8_t kDefaultLedBrightness = static_cast<uint8_t>(FREENOVE_WS2812_BRIGHTNESS);
constexpr float kTwoPi = 6.2831853f;
constexpr float kPitchConfidenceAlpha = 0.45f;
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
constexpr uint16_t kTunerSpectrumBins[HardwareManager::kMicSpectrumBinCount] = {400U, 420U, 440U, 460U, 480U};
constexpr HardwareManager::LedPaletteEntry kLedPalette[] = {
    {"SCENE_LOCKED", 255U, 96U, 22U, 88U, true},
    {"SCENE_BROKEN", 255U, 40U, 18U, 86U, true},
    {"SCENE_U_SON_PROTO", 243U, 93U, 255U, 86U, true},
    {"SCENE_WARNING", 255U, 154U, 74U, 78U, true},
    {"SCENE_SIGNAL_SPIKE", 255U, 40U, 18U, 86U, true},
    {"SCENE_LA_DETECTOR", 32U, 224U, 170U, 56U, true},
    {"SCENE_LEFOU_DETECTOR", 70U, 230U, 200U, 56U, true},
    {"SCENE_SEARCH", 32U, 224U, 170U, 56U, true},
    {"SCENE_WIN", 245U, 205U, 62U, 80U, true},
    {"SCENE_WIN_ETAPE", 245U, 205U, 62U, 80U, true},
    {"SCENE_WIN_ETAPE1", 244U, 203U, 74U, 80U, true},
    {"SCENE_WIN_ETAPE2", 244U, 203U, 74U, 80U, true},
    {"SCENE_FINAL_WIN", 252U, 212U, 92U, 76U, false},
    {"SCENE_REWARD", 245U, 205U, 62U, 80U, true},
    {"SCENE_READY", 18U, 45U, 95U, 52U, false},
    {"SCENE_MP3_PLAYER", 18U, 45U, 95U, 52U, false},
    {"SCENE_MEDIA_MANAGER", 18U, 45U, 95U, 52U, false},
    {"SCENE_PHOTO_MANAGER", 18U, 45U, 95U, 52U, false},
    {"SCENE_CAMERA_SCAN", 18U, 45U, 95U, 52U, false},
    {"SCENE_QR_DETECTOR", 18U, 45U, 95U, 52U, false},
    {"SCENE_TEST_LAB", 0U, 0U, 0U, 0U, false},
    {"SCENE_MEDIA_ARCHIVE", 0U, 0U, 0U, 0U, false},
    {"SCENE_FIREWORKS", 0U, 0U, 0U, 0U, false},
    {"SCENE_WINNER", 0U, 0U, 0U, 0U, false},
    {"__DEFAULT__", 18U, 45U, 95U, 52U, false},
};

struct ScenePaletteAlias {
  const char* alias;
  const char* scene_id;
};

constexpr ScenePaletteAlias kLedPaletteAliases[] = {
    {"SCENE_LA_DETECT", "SCENE_LA_DETECTOR"},
    {"SCENE_U_SON", "SCENE_U_SON_PROTO"},
    {"U_SON_PROTO", "SCENE_U_SON_PROTO"},
    {"SCENE_LE_FOU_DETECTOR", "SCENE_LEFOU_DETECTOR"},
    {"SCENE_LOCK", "SCENE_LOCKED"},
    {"LOCKED", "SCENE_LOCKED"},
    {"LOCK", "SCENE_LOCKED"},
    {"SCENE_AUDIO_PLAYER", "SCENE_MP3_PLAYER"},
    {"SCENE_MP3", "SCENE_MP3_PLAYER"},
};

const char* resolvePaletteSceneId(const char* scene_id) {
  if (scene_id == nullptr || scene_id[0] == '\0') {
    return "SCENE_READY";
  }
  const char* normalized_scene_id = storyNormalizeScreenSceneId(scene_id);
  if (normalized_scene_id != nullptr) {
    return normalized_scene_id;
  }
  for (size_t index = 0U; index < (sizeof(kLedPaletteAliases) / sizeof(kLedPaletteAliases[0])); ++index) {
    const ScenePaletteAlias& alias = kLedPaletteAliases[index];
    if (std::strcmp(alias.alias, scene_id) == 0) {
      return alias.scene_id;
    }
  }
  return scene_id;
}

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

float computeGoertzelPower(const int16_t* samples, size_t sample_count, float target_hz, float sample_rate_hz) {
  if (samples == nullptr || sample_count == 0U || target_hz <= 0.0f || sample_rate_hz <= 0.0f) {
    return 0.0f;
  }
  const float k = 0.5f + ((static_cast<float>(sample_count) * target_hz) / sample_rate_hz);
  const float omega = (kTwoPi * k) / static_cast<float>(sample_count);
  const float coeff = 2.0f * std::cos(omega);
  float q0 = 0.0f;
  float q1 = 0.0f;
  float q2 = 0.0f;
  for (size_t index = 0U; index < sample_count; ++index) {
    q0 = coeff * q1 - q2 + static_cast<float>(samples[index]);
    q2 = q1;
    q1 = q0;
  }
  const float power = q1 * q1 + q2 * q2 - coeff * q1 * q2;
  return (power > 0.0f) ? power : 0.0f;
}

uint8_t computeLevelPercent(uint16_t effective_peak, uint16_t den) {
  const uint8_t raw_level =
      static_cast<uint8_t>(std::min<uint32_t>(100U, (static_cast<uint32_t>(effective_peak) * 100U) / den));
  return raw_level;
}

const char* ledModeName(HardwareManager::LedRuntimeMode mode) {
  switch (mode) {
    case HardwareManager::LedRuntimeMode::kBroken:
      return "broken";
    case HardwareManager::LedRuntimeMode::kTuner:
      return "tuner";
    case HardwareManager::LedRuntimeMode::kSingleRandomBlink:
      return "single_random_blink";
    case HardwareManager::LedRuntimeMode::kPalette:
    default:
      return "palette";
  }
}

void updateLedModeSnapshot(HardwareManager::Snapshot* snapshot,
                           HardwareManager::LedRuntimeMode mode,
                           bool one_led_at_a_time) {
  if (snapshot == nullptr) {
    return;
  }
  snapshot->led_one_at_a_time = one_led_at_a_time;
  std::strncpy(snapshot->led_mode, ledModeName(mode), sizeof(snapshot->led_mode) - 1U);
  snapshot->led_mode[sizeof(snapshot->led_mode) - 1U] = '\0';
}

}  // namespace

HardwareManager::HardwareManager()
    : strip_(FREENOVE_WS2812_COUNT, FREENOVE_WS2812_PIN, NEO_GRB + NEO_KHZ800) {
  snapshot_.led_brightness = kDefaultLedBrightness;
  updateLedModeSnapshot(&snapshot_, LedRuntimeMode::kPalette, false);
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
  snapshot_.mic_ready = snapshot_.mic_ready && mic_enabled_runtime_;
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
  const char* effective_scene_id = resolvePaletteSceneId(scene_id);
  if (std::strncmp(snapshot_.scene_id, effective_scene_id, sizeof(snapshot_.scene_id) - 1U) == 0) {
    return;
  }
  setScenePalette(effective_scene_id);
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

const HardwareManager::Snapshot& HardwareManager::snapshotRef() const {
  return snapshot_;
}

void HardwareManager::setMicRuntimeEnabled(bool enabled) {
  if (mic_enabled_runtime_ == enabled) {
    return;
  }
  mic_enabled_runtime_ = enabled;
  snapshot_.mic_ready = mic_enabled_runtime_ && mic_driver_ready_;
  if (!mic_enabled_runtime_) {
    snapshot_.mic_level_percent = 0U;
    snapshot_.mic_peak = 0U;
    snapshot_.mic_freq_hz = 0U;
    snapshot_.mic_pitch_cents = 0;
    snapshot_.mic_pitch_confidence = 0U;
    snapshot_.mic_waveform_count = 0U;
    snapshot_.mic_waveform_head = 0U;
    std::memset(snapshot_.mic_waveform, 0, sizeof(snapshot_.mic_waveform));
    std::memset(snapshot_.mic_spectrum, 0, sizeof(snapshot_.mic_spectrum));
    snapshot_.mic_spectrum_peak_hz = 0U;
  } else {
    next_mic_ms_ = 0U;
  }
}

bool HardwareManager::micRuntimeEnabled() const {
  return mic_enabled_runtime_;
}

void HardwareManager::setSceneSingleRandomBlink(bool enabled,
                                                uint8_t r,
                                                uint8_t g,
                                                uint8_t b,
                                                uint8_t brightness) {
  scene_single_random_blink_ = enabled;
  scene_single_blink_r_ = r;
  scene_single_blink_g_ = g;
  scene_single_blink_b_ = b;
  scene_single_blink_brightness_ = brightness;
  next_led_ms_ = 0U;
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
  if (!mic_enabled_runtime_) {
    return;
  }
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
  uint16_t smoothed_freq = 0U;
  int16_t smoothed_cents = 0;
  uint8_t smoothed_confidence = 0U;
  applyPitchSmoothing(now_ms,
                      freq_hz,
                      cents,
                      confidence,
                      smoothed_freq,
                      smoothed_cents,
                      smoothed_confidence);

  const bool has_pitch = (smoothed_confidence > 0U) && (smoothed_freq > 0U);

  if (has_pitch) {
    snapshot_.mic_freq_hz = smoothed_freq;
    snapshot_.mic_pitch_cents = smoothed_cents;
    snapshot_.mic_pitch_confidence = smoothed_confidence;
  } else {
    snapshot_.mic_freq_hz = 0U;
    snapshot_.mic_pitch_cents = 0;
    snapshot_.mic_pitch_confidence = 0U;
  }

  const uint16_t level_for_display = computeLevelPercent(effective_peak, kMicAgcMinLevelDen);
  const uint16_t level_for_waveform = (effective_peak >= kMicAgcSignalDisplayPeakMin) ? level_for_display : 0U;
  float spectrum_power[HardwareManager::kMicSpectrumBinCount] = {0.0f};
  float max_spectrum_power = 0.0f;
  uint8_t max_spectrum_index = 0U;
  if (sample_count >= 64U && level_for_display > 0U) {
    for (uint8_t bin = 0U; bin < HardwareManager::kMicSpectrumBinCount; ++bin) {
      const float power =
          computeGoertzelPower(mic_samples_, sample_count, static_cast<float>(kTunerSpectrumBins[bin]), static_cast<float>(kMicSampleRate));
      spectrum_power[bin] = power;
      if (power > max_spectrum_power) {
        max_spectrum_power = power;
        max_spectrum_index = bin;
      }
    }
  }
  if (max_spectrum_power > 0.0f) {
    for (uint8_t bin = 0U; bin < HardwareManager::kMicSpectrumBinCount; ++bin) {
      const float normalized = std::sqrt(spectrum_power[bin] / max_spectrum_power);
      snapshot_.mic_spectrum[bin] = clampU8(static_cast<int>(normalized * 100.0f));
    }
    snapshot_.mic_spectrum_peak_hz = kTunerSpectrumBins[max_spectrum_index];
  } else {
    std::memset(snapshot_.mic_spectrum, 0, sizeof(snapshot_.mic_spectrum));
    snapshot_.mic_spectrum_peak_hz = 0U;
  }
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
  const bool force_scene_palette = false;

  if (manual_led_ && !force_scene_palette) {
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

  if (!manual_led_ && !force_scene_palette && button_flash_until_ms_ <= now_ms && isTunerSceneHint()) {
    led_runtime_mode_ = LedRuntimeMode::kTuner;
    applyTunerLedPattern(now_ms, base_r, base_g, base_b, brightness);
    return;
  }

  if (!manual_led_ && !force_scene_palette && button_flash_until_ms_ <= now_ms && isBrokenSceneHint()) {
    led_runtime_mode_ = LedRuntimeMode::kBroken;
    applyBrokenLedPattern(now_ms, base_r, base_g, base_b, brightness);
    return;
  }

  if (!manual_led_ && !force_scene_palette && button_flash_until_ms_ <= now_ms && scene_single_random_blink_) {
    const uint8_t blink_r = (scene_single_blink_r_ != 0U) ? scene_single_blink_r_ : base_r;
    const uint8_t blink_g = (scene_single_blink_g_ != 0U) ? scene_single_blink_g_ : base_g;
    const uint8_t blink_b = (scene_single_blink_b_ != 0U) ? scene_single_blink_b_ : base_b;
    const uint8_t blink_brightness = (scene_single_blink_brightness_ != 0U) ? scene_single_blink_brightness_ : brightness;
    led_runtime_mode_ = LedRuntimeMode::kSingleRandomBlink;
    applySingleRandomBlinkPattern(now_ms, blink_r, blink_g, blink_b, blink_brightness);
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
  led_runtime_mode_ = LedRuntimeMode::kPalette;
  updateLedModeSnapshot(&snapshot_, led_runtime_mode_, false);
}

bool HardwareManager::isBrokenSceneHint() const {
  return (std::strcmp(snapshot_.scene_id, "SCENE_LOCKED") == 0) ||
         (std::strcmp(snapshot_.scene_id, "SCENE_BROKEN") == 0) ||
         (std::strcmp(snapshot_.scene_id, "SCENE_SIGNAL_SPIKE") == 0);
}

bool HardwareManager::isTunerSceneHint() const {
  return (std::strcmp(snapshot_.scene_id, "SCENE_LA_DETECTOR") == 0) ||
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

  uint8_t peak_r = 0U;
  uint8_t peak_g = 0U;
  uint8_t peak_b = 0U;

  const uint32_t slot = now_ms / 46U;
  const uint32_t in_slot = now_ms % 46U;
  const uint32_t slot_noise = hash32(slot * 2654435761UL + 0x9e3779b9UL);
  const uint16_t primary_led = static_cast<uint16_t>(slot_noise % led_count);
  const uint8_t primary_window_ms = static_cast<uint8_t>(7U + ((slot_noise >> 16) % 11U));
  const bool primary_active = in_slot < primary_window_ms;

  uint16_t secondary_led = primary_led;
  bool secondary_active = false;
#if (FREENOVE_WS2812_COUNT > 1)
  if (led_count > 1U) {
    const uint16_t secondary_span = static_cast<uint16_t>(led_count - 1U);
    const uint16_t secondary_offset = static_cast<uint16_t>((slot_noise >> 8) % secondary_span);
    secondary_led = static_cast<uint16_t>((primary_led + 1U + secondary_offset) % led_count);
    secondary_active = (((slot_noise >> 27) & 0x1U) == 1U) && (in_slot >= 24U) && (in_slot < 29U);
  }
#endif

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

    if (final_r > peak_r) {
      peak_r = final_r;
    }
    if (final_g > peak_g) {
      peak_g = final_g;
    }
    if (final_b > peak_b) {
      peak_b = final_b;
    }
  }

  strip_.show();
  snapshot_.led_r = peak_r;
  snapshot_.led_g = peak_g;
  snapshot_.led_b = peak_b;
  snapshot_.led_brightness = effective_brightness;
  updateLedModeSnapshot(&snapshot_, LedRuntimeMode::kBroken, false);
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

  uint8_t peak_r = 0U;
  uint8_t peak_g = 0U;
  uint8_t peak_b = 0U;

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
    if (out_r > peak_r) {
      peak_r = out_r;
    }
    if (out_g > peak_g) {
      peak_g = out_g;
    }
    if (out_b > peak_b) {
      peak_b = out_b;
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
    snapshot_.led_r = peak_r;
    snapshot_.led_g = peak_g;
    snapshot_.led_b = peak_b;
    snapshot_.led_brightness = tuned_brightness;
    return;
  }

  const uint16_t spectrum_total = static_cast<uint16_t>(snapshot_.mic_spectrum[0]) +
                                  static_cast<uint16_t>(snapshot_.mic_spectrum[1]) +
                                  static_cast<uint16_t>(snapshot_.mic_spectrum[2]) +
                                  static_cast<uint16_t>(snapshot_.mic_spectrum[3]) +
                                  static_cast<uint16_t>(snapshot_.mic_spectrum[4]);
  if (led_count >= 4U && spectrum_total > 0U) {
    const float low_400 = static_cast<float>(snapshot_.mic_spectrum[0]) / 100.0f;
    const float low_420 = static_cast<float>(snapshot_.mic_spectrum[1]) / 100.0f;
    const float mid_440 = static_cast<float>(snapshot_.mic_spectrum[2]) / 100.0f;
    const float high_480 = static_cast<float>(snapshot_.mic_spectrum[4]) / 100.0f;
    const bool in_tune_center = (std::fabs(static_cast<float>(snapshot_.mic_freq_hz) - kTunerReferenceHz) <= 1.8f) &&
                                (snapshot_.mic_pitch_confidence >= 40U);
    const float blink = in_tune_center
                            ? (0.70f + 0.30f * std::sin(static_cast<float>(now_ms % 420U) * (kTwoPi / 420.0f)))
                            : 1.0f;
    setLedScaled(0U, 255U, 18U, 0U, low_400);
    setLedScaled(1U, 255U, 86U, 0U, low_420);
    setLedScaled(2U, 24U, 255U, 88U, mid_440 * blink);
    setLedScaled(3U, 30U, 110U, 255U, high_480);
    for (uint16_t index = 4U; index < led_count; ++index) {
      setLedScaled(index, 0U, 0U, 0U, 0.0f);
    }
    strip_.show();
    snapshot_.led_r = peak_r;
    snapshot_.led_g = peak_g;
    snapshot_.led_b = peak_b;
    snapshot_.led_brightness = tuned_brightness;
    updateLedModeSnapshot(&snapshot_, LedRuntimeMode::kTuner, false);
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
  snapshot_.led_r = peak_r;
  snapshot_.led_g = peak_g;
  snapshot_.led_b = peak_b;
  snapshot_.led_brightness = tuned_brightness;
  updateLedModeSnapshot(&snapshot_, LedRuntimeMode::kTuner, false);
}

void HardwareManager::applySingleRandomBlinkPattern(uint32_t now_ms,
                                                    uint8_t base_r,
                                                    uint8_t base_g,
                                                    uint8_t base_b,
                                                    uint8_t brightness) {
  const uint16_t led_count = FREENOVE_WS2812_COUNT;
  if (led_count == 0U) {
    return;
  }

  uint8_t effective_brightness = brightness;
  if (effective_brightness < 10U) {
    effective_brightness = 10U;
  } else if (effective_brightness > 125U) {
    effective_brightness = 125U;
  }
  strip_.setBrightness(effective_brightness);

  const uint32_t slot = now_ms / 78U;
  const uint32_t in_slot = now_ms % 78U;
  const uint32_t slot_noise = hash32(slot * 2246822519UL + 0x9e3779b9UL);
  const uint16_t active_led = static_cast<uint16_t>(slot_noise % led_count);
  const uint8_t active_window = static_cast<uint8_t>(5U + ((slot_noise >> 16U) % 9U));
  const bool active = in_slot < active_window;
  const float tail = active ? (1.0f - (static_cast<float>(in_slot) / static_cast<float>(active_window))) : 0.0f;

  uint8_t peak_r = 0U;
  uint8_t peak_g = 0U;
  uint8_t peak_b = 0U;
  for (uint16_t index = 0U; index < led_count; ++index) {
    uint8_t out_r = 0U;
    uint8_t out_g = 0U;
    uint8_t out_b = 0U;
    if (active && index == active_led) {
      const float boost = 0.72f + (0.58f * tail);
      out_r = clampU8(static_cast<int>(static_cast<float>(base_r) * boost));
      out_g = clampU8(static_cast<int>(static_cast<float>(base_g) * boost));
      out_b = clampU8(static_cast<int>(static_cast<float>(base_b) * boost));
    }
    strip_.setPixelColor(index, out_r, out_g, out_b);
    if (out_r > peak_r) {
      peak_r = out_r;
    }
    if (out_g > peak_g) {
      peak_g = out_g;
    }
    if (out_b > peak_b) {
      peak_b = out_b;
    }
  }
  strip_.show();

  snapshot_.led_r = peak_r;
  snapshot_.led_g = peak_g;
  snapshot_.led_b = peak_b;
  snapshot_.led_brightness = effective_brightness;
  updateLedModeSnapshot(&snapshot_, LedRuntimeMode::kSingleRandomBlink, true);
}

void HardwareManager::estimatePitch(uint16_t& freq_hz, int16_t& cents, uint8_t& confidence, uint16_t& peak_for_window) {
  freq_hz = snapshot_.mic_freq_hz;
  cents = snapshot_.mic_pitch_cents;
  confidence = snapshot_.mic_pitch_confidence;
  peak_for_window = snapshot_.mic_peak;
}

void HardwareManager::applyPitchSmoothing(uint32_t now_ms,
                                         uint16_t raw_freq,
                                         int16_t raw_cents,
                                         uint8_t raw_confidence,
                                         uint16_t& smoothed_freq,
                                         int16_t& smoothed_cents,
                                         uint8_t& smoothed_confidence) {
  smoothed_freq = 0U;
  smoothed_cents = 0;
  smoothed_confidence = 0U;

  if (raw_freq == 0U || raw_confidence == 0U) {
    if (pitch_smoothing_last_ms_ != 0U && (now_ms - pitch_smoothing_last_ms_) > kPitchSmoothingStaleMs) {
      pitch_confidence_ema_ = 0.0f;
      pitch_smoothing_count_ = 0U;
      pitch_smoothing_index_ = 0U;
      pitch_smoothing_last_ms_ = now_ms;
      return;
    }
    return;
  }

  if (pitch_smoothing_last_ms_ != 0U && (now_ms - pitch_smoothing_last_ms_) > kPitchSmoothingStaleMs) {
    pitch_confidence_ema_ = 0.0f;
    pitch_smoothing_count_ = 0U;
    pitch_smoothing_index_ = 0U;
  }
  pitch_smoothing_last_ms_ = now_ms;

  const uint8_t write_index = pitch_smoothing_index_;
  pitch_freq_window_[write_index] = raw_freq;
  pitch_cents_window_[write_index] = raw_cents;
  pitch_conf_window_[write_index] = raw_confidence;
  pitch_smoothing_index_ = static_cast<uint8_t>((pitch_smoothing_index_ + 1U) % kPitchSmoothingSamples);
  if (pitch_smoothing_count_ < kPitchSmoothingSamples) {
    ++pitch_smoothing_count_;
  }

  uint16_t freq_samples[kPitchSmoothingSamples] = {0U, 0U, 0U};
  int16_t cents_samples[kPitchSmoothingSamples] = {0, 0, 0};
  const uint8_t sample_count = pitch_smoothing_count_;
  const uint8_t oldest_index =
      static_cast<uint8_t>((pitch_smoothing_index_ + (kPitchSmoothingSamples - sample_count)) % kPitchSmoothingSamples);
  for (uint8_t index = 0U; index < sample_count; ++index) {
    const uint8_t src_index = static_cast<uint8_t>((oldest_index + index) % kPitchSmoothingSamples);
    freq_samples[index] = pitch_freq_window_[src_index];
    cents_samples[index] = pitch_cents_window_[src_index];
  }

  std::sort(freq_samples, freq_samples + sample_count);
  std::sort(cents_samples, cents_samples + sample_count);
  const uint8_t median_index = static_cast<uint8_t>(sample_count / 2U);
  smoothed_freq = freq_samples[median_index];
  smoothed_cents = cents_samples[median_index];

  if (pitch_confidence_ema_ <= 0.1f) {
    pitch_confidence_ema_ = static_cast<float>(raw_confidence);
  } else {
    pitch_confidence_ema_ =
        (kPitchConfidenceAlpha * static_cast<float>(raw_confidence)) + ((1.0f - kPitchConfidenceAlpha) * pitch_confidence_ema_);
  }
  const uint8_t smoothed = static_cast<uint8_t>(std::round(pitch_confidence_ema_));
  smoothed_confidence = (smoothed > 100U) ? 100U : smoothed;
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
  const char* effective_scene_id = resolvePaletteSceneId(scene_id);
  std::strncpy(snapshot_.scene_id, effective_scene_id, sizeof(snapshot_.scene_id) - 1U);
  snapshot_.scene_id[sizeof(snapshot_.scene_id) - 1U] = '\0';

  const LedPaletteEntry* palette = findPaletteForScene(effective_scene_id);
  if (palette == nullptr) {
    scene_r_ = 50U;
    scene_g_ = 122U;
    scene_b_ = 255U;
    scene_brightness_ = kDefaultLedBrightness;
    led_pulse_ = true;
    updateLedModeSnapshot(&snapshot_, LedRuntimeMode::kPalette, false);
    return;
  }
  scene_r_ = palette->r;
  scene_g_ = palette->g;
  scene_b_ = palette->b;
  scene_brightness_ = palette->brightness;
  led_pulse_ = palette->pulse;
  updateLedModeSnapshot(&snapshot_, LedRuntimeMode::kPalette, false);
}

const HardwareManager::LedPaletteEntry* HardwareManager::findPaletteForScene(const char* scene_id) const {
  const char* effective_scene_id = resolvePaletteSceneId(scene_id);
  if (effective_scene_id == nullptr || effective_scene_id[0] == '\0') {
    return &kLedPalette[(sizeof(kLedPalette) / sizeof(kLedPalette[0])) - 1U];
  }
  for (size_t index = 0U; index < (sizeof(kLedPalette) / sizeof(kLedPalette[0])); ++index) {
    const LedPaletteEntry& entry = kLedPalette[index];
    if (std::strcmp(entry.scene_id, "__DEFAULT__") == 0) {
      continue;
    }
    if (std::strcmp(entry.scene_id, effective_scene_id) == 0) {
      return &entry;
    }
  }
  return &kLedPalette[(sizeof(kLedPalette) / sizeof(kLedPalette[0])) - 1U];
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
