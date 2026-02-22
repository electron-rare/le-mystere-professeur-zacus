// hardware_manager.h - WS2812 + microphone + battery helpers for Freenove board.
#pragma once

#include <Arduino.h>

#include <Adafruit_NeoPixel.h>
#include <driver/i2s.h>

class HardwareManager {
 public:
  static constexpr uint8_t kMicWaveformCapacity = 16U;

  struct Snapshot {
    bool ready = false;
    bool ws2812_ready = false;
    bool mic_ready = false;
    bool battery_ready = false;
    bool charging = false;
    bool led_manual = false;
    uint8_t led_r = 0U;
    uint8_t led_g = 0U;
    uint8_t led_b = 0U;
    uint8_t led_brightness = 0U;
    uint8_t mic_level_percent = 0U;
    uint16_t mic_peak = 0U;
    uint16_t mic_noise_floor = 0U;
    uint16_t mic_gain_percent = 100U;
    uint16_t mic_freq_hz = 0U;
    int16_t mic_pitch_cents = 0;
    uint8_t mic_pitch_confidence = 0U;
    uint8_t mic_waveform_count = 0U;
    uint8_t mic_waveform_head = 0U;
    uint8_t mic_waveform[kMicWaveformCapacity] = {0};
    uint16_t battery_mv = 0U;
    uint16_t battery_cell_mv = 0U;
    uint8_t battery_percent = 0U;
    uint8_t last_button = 0U;
    bool last_button_long = false;
    uint32_t last_button_ms = 0U;
    uint32_t button_count = 0U;
    char scene_id[24] = "SCENE_READY";
  };

  HardwareManager();

  bool begin();
  void update(uint32_t now_ms);
  void noteButton(uint8_t key, bool long_press, uint32_t now_ms);
  void setSceneHint(const char* scene_id);
  bool setManualLed(uint8_t r, uint8_t g, uint8_t b, uint8_t brightness, bool pulse);
  void clearManualLed();
  Snapshot snapshot() const;

 private:
  bool beginMic();
  void updateMic(uint32_t now_ms);
  void updateBattery(uint32_t now_ms);
  void updateLed(uint32_t now_ms);
  bool isBrokenSceneHint() const;
  bool isTunerSceneHint() const;
  void applyBrokenLedPattern(uint32_t now_ms, uint8_t base_r, uint8_t base_g, uint8_t base_b, uint8_t brightness);
  void applyTunerLedPattern(uint32_t now_ms,
                           uint8_t base_r,
                           uint8_t base_g,
                           uint8_t base_b,
                           uint8_t brightness);
  void estimatePitch(uint16_t& freq_hz, int16_t& cents, uint8_t& confidence, uint16_t& peak_for_window);
  void estimatePitchFromSamples(const int16_t* samples,
                               size_t sample_count,
                               uint16_t& out_freq,
                               int16_t& out_cents,
                               uint8_t& out_confidence);
  void setScenePalette(const char* scene_id);
  uint8_t batteryPercentFromMv(uint16_t cell_mv) const;
  static uint8_t clampColor(int value);

  static constexpr uint16_t kMicSampleRate = 16000U;
  static constexpr uint16_t kMicReadSamples = 256U;
  static constexpr uint32_t kMicPeriodMs = 40U;
  static constexpr uint32_t kBatteryPeriodMs = 1200U;
  static constexpr uint32_t kLedPeriodMs = 33U;
  static constexpr uint32_t kButtonFlashMs = 180U;
  static constexpr i2s_port_t kMicPort = I2S_NUM_1;

  Snapshot snapshot_;
  Adafruit_NeoPixel strip_;

  bool mic_driver_ready_ = false;
  bool led_pulse_ = false;
  uint32_t next_mic_ms_ = 0U;
  uint32_t next_battery_ms_ = 0U;
  uint32_t next_led_ms_ = 0U;
  uint32_t button_flash_until_ms_ = 0U;

  uint8_t scene_r_ = 0U;
  uint8_t scene_g_ = 0U;
  uint8_t scene_b_ = 0U;
  uint8_t scene_brightness_ = 0U;

  bool manual_led_ = false;
  bool manual_pulse_ = false;
  uint8_t manual_r_ = 0U;
  uint8_t manual_g_ = 0U;
  uint8_t manual_b_ = 0U;
  uint8_t manual_brightness_ = 0U;
  uint16_t mic_agc_gain_q8_ = 256U;
  uint16_t mic_noise_floor_raw_ = 120U;
  uint32_t mic_last_signal_ms_ = 0U;

  // Keep DSP buffers off the loop task stack to avoid canary overflows.
  int32_t mic_raw_samples_[kMicReadSamples] = {};
  int16_t mic_samples_[kMicReadSamples] = {};
  float pitch_centered_[kMicReadSamples] = {};
  float pitch_energy_prefix_[kMicReadSamples + 1U] = {};
  float pitch_corr_by_lag_[kMicReadSamples + 1U] = {};
};
