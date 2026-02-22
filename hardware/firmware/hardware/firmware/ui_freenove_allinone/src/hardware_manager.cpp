// hardware_manager.cpp - Freenove peripherals (WS2812, mic, battery, buttons).
#include "hardware_manager.h"

#include <cmath>
#include <cstring>

#include "ui_freenove_config.h"

namespace {

constexpr uint8_t kDefaultLedBrightness = static_cast<uint8_t>(FREENOVE_WS2812_BRIGHTNESS);
constexpr float kTwoPi = 6.2831853f;

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

}  // namespace

HardwareManager::HardwareManager()
    : strip_(FREENOVE_WS2812_COUNT, FREENOVE_WS2812_PIN, NEO_GRB + NEO_KHZ800) {
  snapshot_.led_brightness = kDefaultLedBrightness;
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
  config.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
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

  if (i2s_set_clk(kMicPort, kMicSampleRate, I2S_BITS_PER_SAMPLE_16BIT, I2S_CHANNEL_MONO) != ESP_OK) {
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

  int16_t samples[kMicReadSamples] = {};
  size_t bytes_read = 0U;
  if (i2s_read(kMicPort, samples, sizeof(samples), &bytes_read, 0) != ESP_OK || bytes_read == 0U) {
    return;
  }

  const size_t sample_count = bytes_read / sizeof(int16_t);
  uint16_t peak = 0U;
  for (size_t index = 0U; index < sample_count; ++index) {
    int value = static_cast<int>(samples[index]);
    if (value < 0) {
      value = -value;
    }
    if (value > peak) {
      peak = static_cast<uint16_t>(value);
    }
  }

  uint8_t level = static_cast<uint8_t>((static_cast<uint32_t>(peak) * 100U) / 12000U);
  if (level > 100U) {
    level = 100U;
  }
  snapshot_.mic_peak = peak;
  snapshot_.mic_level_percent = static_cast<uint8_t>((snapshot_.mic_level_percent * 3U + level) / 4U);
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
