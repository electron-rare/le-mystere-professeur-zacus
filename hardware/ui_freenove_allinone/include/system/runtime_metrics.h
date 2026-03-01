#pragma once

#include <Arduino.h>

#include <cstdint>

#if defined(ARDUINO_ARCH_ESP32)
#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>
#endif

struct RuntimeMetricsSnapshot {
  uint32_t reset_reason = 0U;
  uint32_t audio_underrun = 0U;
  uint32_t sd_errors = 0U;
  uint32_t ui_fps_approx = 0U;
  uint32_t ui_frame_count = 0U;
};

class RuntimeMetrics {
 public:
  static RuntimeMetrics& instance();

  void reset(uint32_t reset_reason_code);
  void setResetReason(uint32_t reset_reason_code);
  void noteAudioUnderrun(uint32_t count = 1U);
  void noteSdError(uint32_t count = 1U);
  void noteUiFrame(uint32_t now_ms);
  RuntimeMetricsSnapshot snapshot() const;
  void logPeriodic(uint32_t now_ms, uint32_t interval_ms = 5000U);

 private:
  RuntimeMetrics() = default;

  void lock() const;
  void unlock() const;

  uint32_t reset_reason_ = 0U;
  uint32_t audio_underrun_ = 0U;
  uint32_t sd_errors_ = 0U;
  uint32_t ui_fps_approx_ = 0U;
  uint32_t ui_frame_count_ = 0U;
  uint32_t ui_fps_window_start_ms_ = 0U;
  uint32_t ui_fps_window_frames_ = 0U;
  uint32_t last_log_ms_ = 0U;

#if defined(ARDUINO_ARCH_ESP32)
  mutable portMUX_TYPE lock_ = portMUX_INITIALIZER_UNLOCKED;
#endif
};
