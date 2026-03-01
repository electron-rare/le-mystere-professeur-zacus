#include "system/runtime_metrics.h"

RuntimeMetrics& RuntimeMetrics::instance() {
  static RuntimeMetrics metrics;
  return metrics;
}

void RuntimeMetrics::lock() const {
#if defined(ARDUINO_ARCH_ESP32)
  portENTER_CRITICAL(&lock_);
#endif
}

void RuntimeMetrics::unlock() const {
#if defined(ARDUINO_ARCH_ESP32)
  portEXIT_CRITICAL(&lock_);
#endif
}

void RuntimeMetrics::reset(uint32_t reset_reason_code) {
  lock();
  reset_reason_ = reset_reason_code;
  audio_underrun_ = 0U;
  sd_errors_ = 0U;
  ui_fps_approx_ = 0U;
  ui_frame_count_ = 0U;
  ui_fps_window_start_ms_ = 0U;
  ui_fps_window_frames_ = 0U;
  last_log_ms_ = 0U;
  unlock();
}

void RuntimeMetrics::setResetReason(uint32_t reset_reason_code) {
  lock();
  reset_reason_ = reset_reason_code;
  unlock();
}

void RuntimeMetrics::noteAudioUnderrun(uint32_t count) {
  if (count == 0U) {
    return;
  }
  lock();
  audio_underrun_ += count;
  unlock();
}

void RuntimeMetrics::noteSdError(uint32_t count) {
  if (count == 0U) {
    return;
  }
  lock();
  sd_errors_ += count;
  unlock();
}

void RuntimeMetrics::noteUiFrame(uint32_t now_ms) {
  lock();
  ++ui_frame_count_;
  if (ui_fps_window_start_ms_ == 0U) {
    ui_fps_window_start_ms_ = now_ms;
    ui_fps_window_frames_ = 0U;
  }
  ++ui_fps_window_frames_;

  const uint32_t elapsed_ms = now_ms - ui_fps_window_start_ms_;
  if (elapsed_ms >= 1000U) {
    ui_fps_approx_ = static_cast<uint32_t>((ui_fps_window_frames_ * 1000UL) / elapsed_ms);
    ui_fps_window_start_ms_ = now_ms;
    ui_fps_window_frames_ = 0U;
  }
  unlock();
}

RuntimeMetricsSnapshot RuntimeMetrics::snapshot() const {
  RuntimeMetricsSnapshot out = {};
  lock();
  out.reset_reason = reset_reason_;
  out.audio_underrun = audio_underrun_;
  out.sd_errors = sd_errors_;
  out.ui_fps_approx = ui_fps_approx_;
  out.ui_frame_count = ui_frame_count_;
  unlock();
  return out;
}

void RuntimeMetrics::logPeriodic(uint32_t now_ms, uint32_t interval_ms) {
  if (interval_ms == 0U) {
    return;
  }

  RuntimeMetricsSnapshot snap = {};
  bool should_log = false;

  lock();
  if (last_log_ms_ == 0U || (now_ms - last_log_ms_) >= interval_ms) {
    last_log_ms_ = now_ms;
    should_log = true;
    snap.reset_reason = reset_reason_;
    snap.audio_underrun = audio_underrun_;
    snap.sd_errors = sd_errors_;
    snap.ui_fps_approx = ui_fps_approx_;
    snap.ui_frame_count = ui_frame_count_;
  }
  unlock();

  if (!should_log) {
    return;
  }

  Serial.printf("[METRICS] reset=%lu ui_fps=%lu ui_frames=%lu audio_underrun=%lu sd_errors=%lu\n",
                static_cast<unsigned long>(snap.reset_reason),
                static_cast<unsigned long>(snap.ui_fps_approx),
                static_cast<unsigned long>(snap.ui_frame_count),
                static_cast<unsigned long>(snap.audio_underrun),
                static_cast<unsigned long>(snap.sd_errors));
}
