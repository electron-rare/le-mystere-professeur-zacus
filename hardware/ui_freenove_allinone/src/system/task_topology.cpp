#include "system/task_topology.h"

#include <Arduino.h>

namespace fw_system {

TaskTopology& TaskTopology::instance() {
  static TaskTopology topology;
  return topology;
}

#if defined(ARDUINO_ARCH_ESP32)
void TaskTopology::taskThunk(void* arg) {
  TaskLaunch* launch = static_cast<TaskLaunch*>(arg);
  if (launch != nullptr && launch->fn != nullptr) {
    launch->fn(launch->context);
  }
  vTaskDelete(nullptr);
}
#endif

bool TaskTopology::begin(const TaskTopologyConfig& config, const Callbacks& callbacks) {
  if (!config.enabled) {
    return false;
  }
  if (running_) {
    return true;
  }

#if defined(ARDUINO_ARCH_ESP32)
  ui_launch_.fn = callbacks.ui;
  ui_launch_.context = callbacks.context;
  audio_launch_.fn = callbacks.audio;
  audio_launch_.context = callbacks.context;
  storage_launch_.fn = callbacks.storage;
  storage_launch_.context = callbacks.context;
  camera_launch_.fn = callbacks.camera;
  camera_launch_.context = callbacks.context;

  auto create_task = [](TaskEntry fn,
                        TaskLaunch* launch,
                        const char* name,
                        const uint32_t stack_words,
                        const UBaseType_t priority,
                        TaskHandle_t* handle,
                        const BaseType_t core) -> bool {
    if (fn == nullptr) {
      return true;
    }
    return xTaskCreatePinnedToCore(taskThunk,
                                   name,
                                   stack_words,
                                   launch,
                                   priority,
                                   handle,
                                   core) == pdPASS;
  };

  if (!create_task(callbacks.ui,
                   &ui_launch_,
                   "ui_task",
                   kUiStackWords,
                   kUiPriority,
                   &ui_task_,
                   kUiCore) ||
      !create_task(callbacks.audio,
                   &audio_launch_,
                   "audio_task",
                   kAudioStackWords,
                   kAudioPriority,
                   &audio_task_,
                   kAudioCore) ||
      !create_task(callbacks.storage,
                   &storage_launch_,
                   "storage_task",
                   kStorageStackWords,
                   kStoragePriority,
                   &storage_task_,
                   kStorageCore) ||
      !create_task(callbacks.camera,
                   &camera_launch_,
                   "camera_task",
                   kCameraStackWords,
                   kCameraPriority,
                   &camera_task_,
                   kCameraCore)) {
    stop();
    Serial.println("[TASK] topology init failed");
    return false;
  }

  running_ = true;
  Serial.printf("[TASK] topology started ui=%u audio=%u storage=%u camera=%u\n",
                callbacks.ui != nullptr ? 1U : 0U,
                callbacks.audio != nullptr ? 1U : 0U,
                callbacks.storage != nullptr ? 1U : 0U,
                callbacks.camera != nullptr ? 1U : 0U);
  return true;
#else
  (void)callbacks;
  return false;
#endif
}

void TaskTopology::stop() {
#if defined(ARDUINO_ARCH_ESP32)
  if (ui_task_ != nullptr) {
    vTaskDelete(ui_task_);
    ui_task_ = nullptr;
  }
  if (audio_task_ != nullptr) {
    vTaskDelete(audio_task_);
    audio_task_ = nullptr;
  }
  if (storage_task_ != nullptr) {
    vTaskDelete(storage_task_);
    storage_task_ = nullptr;
  }
  if (camera_task_ != nullptr) {
    vTaskDelete(camera_task_);
    camera_task_ = nullptr;
  }
#endif
  running_ = false;
}

bool TaskTopology::running() const {
  return running_;
}

}  // namespace fw_system
