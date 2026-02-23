// button_manager.h - button scan + long press detection.
#pragma once

#include <Arduino.h>

#if defined(ARDUINO_ARCH_ESP32)
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#endif

struct ButtonEvent {
  uint8_t key = 0U;
  bool long_press = false;
  uint32_t ms = 0U;
};

class ButtonManager {
 public:
  bool begin();
  bool pollEvent(ButtonEvent* out_event);

  bool isPressed(uint8_t key) const;
  uint8_t currentKey() const;
  int lastAnalogMilliVolts() const;

 private:
  bool startScanTask();
  void stopScanTask();
  bool runScan(ButtonEvent* out_event);
  bool scanOnce(ButtonEvent* out_event);

#if defined(ARDUINO_ARCH_ESP32)
  static void scanTaskEntry(void* arg);
  void scanTaskMain();

  static constexpr uint16_t kScanTaskStackWords = 2048U;
  static constexpr uint8_t kScanTaskPriority = 1U;
  static constexpr int8_t kScanTaskCore = 0;
  static constexpr uint16_t kScanTaskDelayMs = 5U;
  static constexpr uint8_t kButtonEventQueueDepth = 8U;
#endif

#if defined(ARDUINO_ARCH_ESP32)
  QueueHandle_t event_queue_ = nullptr;
  TaskHandle_t scan_task_ = nullptr;
  bool scan_task_running_ = false;
  mutable portMUX_TYPE state_lock_ = portMUX_INITIALIZER_UNLOCKED;
#else
  // On non-ESP32 builds, keep synchronous polling without RTOS objects.
  bool scan_task_running_ = false;
#endif

  void lockState() const;
  void unlockState() const;

  uint8_t decodeAnalogKey(int millivolts) const;
  bool pollAnalog(ButtonEvent* out_event);
  bool pollDigital(ButtonEvent* out_event);

  bool analog_mode_ = false;
  int last_analog_mv_ = -1;
  int voltage_thresholds_[6] = {0, 447, 730, 1008, 1307, 1659};
  int threshold_range_mv_ = 70;

  uint8_t analog_key_ = 0U;
  uint8_t analog_raw_key_ = 0U;
  uint32_t analog_pressed_at_ms_ = 0U;
  uint32_t analog_raw_changed_ms_ = 0U;

  bool digital_pressed_[4] = {false, false, false, false};
  uint32_t digital_pressed_at_ms_[4] = {0U, 0U, 0U, 0U};
};
