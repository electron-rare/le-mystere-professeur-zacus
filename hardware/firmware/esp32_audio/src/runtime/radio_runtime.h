#pragma once

#include <Arduino.h>

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/queue.h>
#include <freertos/task.h>

class WifiService;
class RadioService;
class WebUiService;

class RadioRuntime {
 public:
  enum class CommandType : uint8_t {
    kNone = 0,
    kRefreshStatus,
    kScanWifi,
  };

  struct Command {
    CommandType type = CommandType::kNone;
    uint32_t arg = 0U;
  };

  struct Metrics {
    bool enabled = false;
    bool started = false;
    uint32_t cmdQueued = 0U;
    uint32_t cmdDropped = 0U;
    uint32_t evtPushed = 0U;
    uint32_t taskCreateFail = 0U;
    uint32_t streamTicks = 0U;
    uint32_t webTicks = 0U;
    uint32_t uiTicks = 0U;
    uint32_t storageTicks = 0U;
    uint32_t audioTicks = 0U;
  };

  struct TaskSnapshot {
    const char* name = nullptr;
    uint32_t stackMinWords = 0U;
    uint32_t stackMinBytes = 0U;
    uint32_t ticks = 0U;
    uint32_t lastTickMs = 0U;
    uint8_t core = 0U;
  };

  void begin(bool enableTasks, WifiService* wifi, RadioService* radio, WebUiService* web);
  void updateCooperative(uint32_t nowMs);
  bool enqueueCommand(const Command& cmd);
  Metrics metrics() const;
  size_t taskSnapshots(TaskSnapshot* out, size_t max) const;
  bool enabled() const;
  bool started() const;

 private:
  static void taskAudioThunk(void* arg);
  static void taskStreamThunk(void* arg);
  static void taskStorageThunk(void* arg);
  static void taskWebThunk(void* arg);
  static void taskUiThunk(void* arg);

  void taskAudioLoop();
  void taskStreamLoop();
  void taskStorageLoop();
  void taskWebLoop();
  void taskUiLoop();

  void createTasks();

  WifiService* wifi_ = nullptr;
  RadioService* radio_ = nullptr;
  WebUiService* web_ = nullptr;

  bool enabled_ = false;
  bool started_ = false;
  QueueHandle_t cmdQueue_ = nullptr;
  QueueHandle_t evtQueue_ = nullptr;
  EventGroupHandle_t flags_ = nullptr;
  TaskHandle_t taskAudio_ = nullptr;
  TaskHandle_t taskStream_ = nullptr;
  TaskHandle_t taskStorage_ = nullptr;
  TaskHandle_t taskWeb_ = nullptr;
  TaskHandle_t taskUi_ = nullptr;
  uint32_t lastAudioTickMs_ = 0U;
  uint32_t lastStreamTickMs_ = 0U;
  uint32_t lastStorageTickMs_ = 0U;
  uint32_t lastWebTickMs_ = 0U;
  uint32_t lastUiTickMs_ = 0U;
  bool wdtEnabled_ = false;
  Metrics metrics_;
};
