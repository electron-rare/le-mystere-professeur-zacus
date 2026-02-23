// button_manager.cpp - button scan + long press detection.
#include "button_manager.h"

#include "ui_freenove_config.h"

namespace {

constexpr uint32_t kDebounceMs = 30U;
constexpr uint32_t kLongPressMs = FREENOVE_BTN_LONG_PRESS_MS;
constexpr int kNoAnalogButtonMv = 2800;

const int kDigitalButtonPins[4] = {
    FREENOVE_BTN_1,
    FREENOVE_BTN_2,
    FREENOVE_BTN_3,
    FREENOVE_BTN_4,
};

}  // namespace

bool ButtonManager::begin() {
  scan_task_running_ = false;
#if FREENOVE_BTN_ANALOG_PIN >= 0
  analog_mode_ = true;
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);
  pinMode(FREENOVE_BTN_ANALOG_PIN, INPUT);
  lockState();
  analog_key_ = 0U;
  analog_raw_key_ = 0U;
  analog_pressed_at_ms_ = 0U;
  analog_raw_changed_ms_ = millis();
  last_analog_mv_ = kNoAnalogButtonMv;
  unlockState();
  Serial.printf("[BTN] analog ladder mode on GPIO %d\n", FREENOVE_BTN_ANALOG_PIN);
#else
  analog_mode_ = false;
  last_analog_mv_ = -1;
  lockState();
  for (uint8_t index = 0; index < 4; ++index) {
    if (kDigitalButtonPins[index] >= 0) {
      pinMode(kDigitalButtonPins[index], INPUT_PULLUP);
      digital_pressed_[index] = false;
      digital_pressed_at_ms_[index] = 0U;
    }
  }
  unlockState();
  Serial.println("[BTN] digital mode");
#endif
  if (!startScanTask()) {
    Serial.println("[BTN] async scan task not started: fallback to sync polling");
  } else {
    Serial.println("[BTN] async scan task started");
  }
  return true;
}

bool ButtonManager::pollEvent(ButtonEvent* out_event) {
  if (out_event == nullptr) {
    return false;
  }
#if defined(ARDUINO_ARCH_ESP32)
  if (scan_task_running_ && event_queue_ != nullptr) {
    if (xQueueReceive(event_queue_, out_event, 0U) == pdTRUE) {
      return true;
    }
    return false;
  }
#endif
  return runScan(out_event);
}

bool ButtonManager::runScan(ButtonEvent* out_event) {
  if (out_event == nullptr) {
    return false;
  }
  if (analog_mode_) {
    return pollAnalog(out_event);
  }
  return pollDigital(out_event);
}

bool ButtonManager::scanOnce(ButtonEvent* out_event) {
  if (out_event == nullptr) {
    return false;
  }
  if (!runScan(out_event)) {
    return false;
  }
  if (out_event->ms == 0U) {
    out_event->ms = millis();
  }
  return true;
}

void ButtonManager::scanTaskMain() {
  while (true) {
    ButtonEvent event;
    if (scanOnce(&event)) {
      if (event_queue_ != nullptr) {
        xQueueSend(event_queue_, &event, 0U);
      }
    }
    vTaskDelay(pdMS_TO_TICKS(kScanTaskDelayMs));
  }
}

void ButtonManager::scanTaskEntry(void* arg) {
  auto* self = static_cast<ButtonManager*>(arg);
  if (self == nullptr) {
    vTaskDelete(nullptr);
    return;
  }
  self->scanTaskMain();
}

bool ButtonManager::startScanTask() {
#if !defined(ARDUINO_ARCH_ESP32)
  return false;
#else
  if (scan_task_running_) {
    return true;
  }
  event_queue_ = xQueueCreate(kButtonEventQueueDepth, sizeof(ButtonEvent));
  if (event_queue_ == nullptr) {
    return false;
  }
  const BaseType_t created =
      xTaskCreatePinnedToCore(scanTaskEntry, "btn_scan", kScanTaskStackWords, this, kScanTaskPriority, &scan_task_, kScanTaskCore);
  if (created != pdPASS) {
    vQueueDelete(event_queue_);
    event_queue_ = nullptr;
    return false;
  }
  scan_task_running_ = true;
  return true;
#endif
}

void ButtonManager::stopScanTask() {
#if defined(ARDUINO_ARCH_ESP32)
  if (!scan_task_running_) {
    return;
  }
  if (scan_task_ != nullptr) {
    vTaskDelete(scan_task_);
  }
  scan_task_running_ = false;
  scan_task_ = nullptr;
  if (event_queue_ != nullptr) {
    vQueueDelete(event_queue_);
    event_queue_ = nullptr;
  }
#endif
}

void ButtonManager::lockState() const {
#if defined(ARDUINO_ARCH_ESP32)
  portENTER_CRITICAL(&state_lock_);
#endif
}

void ButtonManager::unlockState() const {
#if defined(ARDUINO_ARCH_ESP32)
  portEXIT_CRITICAL(&state_lock_);
#endif
}

bool ButtonManager::isPressed(uint8_t key) const {
  if (key < 1U || key > 5U) {
    return false;
  }
  lockState();
  if (analog_mode_) {
    const bool pressed = (analog_key_ == key);
    unlockState();
    return pressed;
  }
  if (key > 4U) {
    unlockState();
    return false;
  }
  const bool pressed = digital_pressed_[key - 1U];
  unlockState();
  return pressed;
}

uint8_t ButtonManager::currentKey() const {
  lockState();
  if (analog_mode_) {
    const uint8_t key = analog_key_;
    unlockState();
    return key;
  }
  uint8_t key = 0U;
  for (uint8_t index = 0; index < 4; ++index) {
    if (digital_pressed_[index]) {
      key = static_cast<uint8_t>(index + 1U);
      break;
    }
  }
  unlockState();
  return key;
}

int ButtonManager::lastAnalogMilliVolts() const {
  lockState();
  const int mv = last_analog_mv_;
  unlockState();
  return mv;
}

uint8_t ButtonManager::decodeAnalogKey(int millivolts) const {
  if (millivolts < 0) {
    return 0U;
  }

  const int no_button_floor_mv = kNoAnalogButtonMv - threshold_range_mv_;
  if (millivolts >= no_button_floor_mv) {
    return 0U;
  }

  // Prefer midpoint buckets between nominal ladder voltages.
  const int split_12 = (voltage_thresholds_[1] + voltage_thresholds_[2]) / 2;
  const int split_23 = (voltage_thresholds_[2] + voltage_thresholds_[3]) / 2;
  const int split_34 = (voltage_thresholds_[3] + voltage_thresholds_[4]) / 2;
  const int split_45 = (voltage_thresholds_[4] + voltage_thresholds_[5]) / 2;
  const int split_5n = (voltage_thresholds_[5] + no_button_floor_mv) / 2;
  if (millivolts <= split_12) {
    return 1U;
  }
  if (millivolts <= split_23) {
    return 2U;
  }
  if (millivolts <= split_34) {
    return 3U;
  }
  if (millivolts <= split_45) {
    return 4U;
  }
  if (millivolts <= split_5n) {
    return 5U;
  }

  // Fallback nearest-threshold match with wider tolerance for board variance.
  int best_key = 1;
  int best_delta = millivolts - voltage_thresholds_[1];
  if (best_delta < 0) {
    best_delta = -best_delta;
  }
  for (int index = 2; index <= 5; ++index) {
    int delta = millivolts - voltage_thresholds_[index];
    if (delta < 0) {
      delta = -delta;
    }
    if (delta < best_delta) {
      best_delta = delta;
      best_key = index;
    }
  }
  if (best_delta <= (threshold_range_mv_ * 7)) {
    return static_cast<uint8_t>(best_key);
  }
  return 0U;
}

bool ButtonManager::pollAnalog(ButtonEvent* out_event) {
#if FREENOVE_BTN_ANALOG_PIN < 0
  (void)out_event;
  return false;
#else
  const int analog_mv = analogReadMilliVolts(FREENOVE_BTN_ANALOG_PIN);
  const uint32_t now_ms = millis();
  const uint8_t raw_key = decodeAnalogKey(analog_mv);

  lockState();
  last_analog_mv_ = analog_mv;
  if (raw_key != analog_raw_key_) {
    analog_raw_key_ = raw_key;
    analog_raw_changed_ms_ = now_ms;
  }
  const uint32_t raw_changed_ms = analog_raw_changed_ms_;
  const uint8_t stable_key = analog_raw_key_;
  const uint8_t pressed_key = analog_key_;
  const uint32_t pressed_at_ms = analog_pressed_at_ms_;
  if ((now_ms - raw_changed_ms) < kDebounceMs) {
    unlockState();
    return false;
  }
  if (stable_key == pressed_key) {
    unlockState();
    return false;
  }
  if (pressed_key == 0U && stable_key > 0U) {
    analog_key_ = stable_key;
    analog_pressed_at_ms_ = now_ms;
    unlockState();
    return false;
  }
  if (pressed_key == 0U) {
    unlockState();
    return false;
  }

  const uint32_t hold_ms = now_ms - pressed_at_ms;
  analog_key_ = stable_key;
  analog_pressed_at_ms_ = (stable_key > 0U) ? now_ms : 0U;
  unlockState();

  if (hold_ms < kDebounceMs) {
    return false;
  }
  out_event->key = pressed_key;
  out_event->long_press = hold_ms >= kLongPressMs;
  out_event->ms = now_ms;
  return true;
#endif
}

bool ButtonManager::pollDigital(ButtonEvent* out_event) {
  const uint32_t now_ms = millis();
  bool pressed[4] = {false, false, false, false};
  for (uint8_t index = 0; index < 4; ++index) {
    if (kDigitalButtonPins[index] < 0) {
      continue;
    }
    pressed[index] = (digitalRead(kDigitalButtonPins[index]) == LOW);
  }

  lockState();
  for (uint8_t index = 0; index < 4; ++index) {
    if (kDigitalButtonPins[index] < 0) {
      continue;
    }
    if (!pressed[index] && digital_pressed_[index]) {
      const uint32_t hold_ms = now_ms - digital_pressed_at_ms_[index];
      digital_pressed_[index] = false;
      if (hold_ms < kDebounceMs) {
        continue;
      }
      out_event->key = static_cast<uint8_t>(index + 1U);
      out_event->long_press = hold_ms >= kLongPressMs;
      out_event->ms = now_ms;
      unlockState();
      return true;
    }
    if (pressed[index] && !digital_pressed_[index]) {
      digital_pressed_[index] = true;
      digital_pressed_at_ms_[index] = now_ms;
    }
  }
  unlockState();
  return false;
}
