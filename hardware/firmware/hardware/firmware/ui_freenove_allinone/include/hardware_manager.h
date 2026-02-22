// hardware_manager.h - WS2812 + microphone + battery helpers for Freenove board.
#pragma once

#include <Arduino.h>

#include <Adafruit_NeoPixel.h>
#include <driver/i2s.h>

class HardwareManager {
 public:
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
};
