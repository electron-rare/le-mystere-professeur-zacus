#pragma once

#include <cstdint>

#if defined(ARDUINO_ARCH_ESP32)
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#else
using UBaseType_t = uint32_t;
using BaseType_t = int32_t;
#endif

namespace fw_system {

struct TaskTopologyConfig {
  bool enabled = false;
};

class TaskTopology {
 public:
  using TaskEntry = void (*)(void* context);

  struct Callbacks {
    TaskEntry ui = nullptr;
    TaskEntry audio = nullptr;
    TaskEntry storage = nullptr;
    TaskEntry camera = nullptr;
    void* context = nullptr;
  };

  static constexpr uint16_t kUiStackWords = 6144U;
  static constexpr uint16_t kAudioStackWords = 6144U;
  static constexpr uint16_t kStorageStackWords = 4096U;
  static constexpr uint16_t kCameraStackWords = 4096U;

  static constexpr UBaseType_t kUiPriority = 4U;
  static constexpr UBaseType_t kAudioPriority = 5U;
  static constexpr UBaseType_t kStoragePriority = 3U;
  static constexpr UBaseType_t kCameraPriority = 3U;

  static constexpr BaseType_t kUiCore = 1;
  static constexpr BaseType_t kAudioCore = 1;
  static constexpr BaseType_t kStorageCore = 0;
  static constexpr BaseType_t kCameraCore = 0;

  static constexpr uint8_t kUiCommandQueueDepth = 16U;
  static constexpr uint8_t kAudioCommandQueueDepth = 8U;
  static constexpr uint8_t kStoragePrefetchQueueDepth = 4U;
  static constexpr uint8_t kCameraFrameQueueDepth = 4U;

  static TaskTopology& instance();

  bool begin(const TaskTopologyConfig& config, const Callbacks& callbacks);
  void stop();
  bool running() const;

  TaskTopology(const TaskTopology&) = delete;
  TaskTopology& operator=(const TaskTopology&) = delete;

 private:
  TaskTopology() = default;

#if defined(ARDUINO_ARCH_ESP32)
  struct TaskLaunch {
    TaskEntry fn = nullptr;
    void* context = nullptr;
  };

  static void taskThunk(void* arg);

  TaskHandle_t ui_task_ = nullptr;
  TaskHandle_t audio_task_ = nullptr;
  TaskHandle_t storage_task_ = nullptr;
  TaskHandle_t camera_task_ = nullptr;

  TaskLaunch ui_launch_ = {};
  TaskLaunch audio_launch_ = {};
  TaskLaunch storage_launch_ = {};
  TaskLaunch camera_launch_ = {};
#endif

  bool running_ = false;
};

}  // namespace fw_system
