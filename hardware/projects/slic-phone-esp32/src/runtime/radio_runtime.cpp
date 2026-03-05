#include "radio_runtime.h"

#include <esp_task_wdt.h>

#include "../config.h"
#include "../services/network/wifi_service.h"
#include "../services/radio/radio_service.h"
#include "../services/web/web_ui_service.h"

namespace {

constexpr uint16_t kCmdQueueLen = 24U;
constexpr uint16_t kEvtQueueLen = 24U;
constexpr uint32_t kTickAudioMs = 15U;
constexpr uint32_t kTickStreamMs = 20U;
constexpr uint32_t kTickStorageMs = 40U;
constexpr uint32_t kTickWebMs = 15U;
constexpr uint32_t kTickUiMs = 20U;
constexpr uint8_t kTaskCoreAudio = 1U;
constexpr uint8_t kTaskCoreUi = 1U;
constexpr uint8_t kTaskCoreNet = 0U;
constexpr uint8_t kTaskCoreStorage = 0U;
constexpr uint8_t kTaskCoreWeb = 0U;

void registerWdtTask(TaskHandle_t handle, const char* name) {
  if (handle == nullptr) {
    return;
  }
  const esp_err_t err = esp_task_wdt_add(handle);
  if (err != ESP_OK) {
    Serial.printf("[RTOS] WDT add failed: %s err=%d\n", name != nullptr ? name : "task", err);
  }
}

}  // namespace

void RadioRuntime::begin(bool enableTasks, WifiService* wifi, RadioService* radio, WebUiService* web) {
  wifi_ = wifi;
  radio_ = radio;
  web_ = web;

  metrics_ = Metrics();
  metrics_.enabled = enableTasks;
  enabled_ = enableTasks;

  if (!enabled_) {
    started_ = true;
    metrics_.started = true;
    return;
  }

  cmdQueue_ = xQueueCreate(kCmdQueueLen, sizeof(Command));
  evtQueue_ = xQueueCreate(kEvtQueueLen, sizeof(Command));
  flags_ = xEventGroupCreate();

  if (cmdQueue_ == nullptr || evtQueue_ == nullptr || flags_ == nullptr) {
    enabled_ = false;
    metrics_.enabled = false;
    started_ = true;
    metrics_.started = true;
    return;
  }

  createTasks();
  if (config::kEnableRadioRuntimeWdt) {
    const esp_err_t initErr = esp_task_wdt_init(config::kRadioRuntimeWdtTimeoutSec, true);
    if (initErr == ESP_OK || initErr == ESP_ERR_INVALID_STATE) {
      wdtEnabled_ = true;
      registerWdtTask(taskAudio_, "TaskAudioEngine");
      registerWdtTask(taskStream_, "TaskStreamNet");
      registerWdtTask(taskStorage_, "TaskStorageScan");
      registerWdtTask(taskWeb_, "TaskWebControl");
      registerWdtTask(taskUi_, "TaskUiOrchestrator");
    } else {
      Serial.printf("[RTOS] WDT init failed err=%d\n", initErr);
    }
  }
  started_ = true;
  metrics_.started = true;
}

void RadioRuntime::updateCooperative(uint32_t nowMs) {
  if (!started_ || enabled_) {
    return;
  }
  if (wifi_ != nullptr) {
    wifi_->update(nowMs);
  }
  if (radio_ != nullptr) {
    radio_->update(nowMs);
  }
  if (web_ != nullptr) {
    web_->update(nowMs);
  }
}

bool RadioRuntime::enqueueCommand(const Command& cmd) {
  if (cmdQueue_ == nullptr) {
    ++metrics_.cmdDropped;
    return false;
  }
  const BaseType_t ok = xQueueSend(cmdQueue_, &cmd, 0U);
  if (ok == pdTRUE) {
    ++metrics_.cmdQueued;
    return true;
  }
  ++metrics_.cmdDropped;
  return false;
}

RadioRuntime::Metrics RadioRuntime::metrics() const {
  return metrics_;
}

void RadioRuntime::createTasks() {
  // Core ownership policy:
  // - Core 1: audio + ui orchestration
  // - Core 0: stream/net + storage + web control
  if (xTaskCreatePinnedToCore(taskAudioThunk, "TaskAudioEngine", 3072, this, 4, &taskAudio_, 1) != pdPASS) {
    ++metrics_.taskCreateFail;
    taskAudio_ = nullptr;
    Serial.println("[RTOS] task create failed: TaskAudioEngine");
  }
  if (xTaskCreatePinnedToCore(taskStreamThunk, "TaskStreamNet", 4096, this, 3, &taskStream_, 0) != pdPASS) {
    ++metrics_.taskCreateFail;
    taskStream_ = nullptr;
    Serial.println("[RTOS] task create failed: TaskStreamNet");
  }
  if (xTaskCreatePinnedToCore(taskStorageThunk, "TaskStorageScan", 3072, this, 2, &taskStorage_, 0) != pdPASS) {
    ++metrics_.taskCreateFail;
    taskStorage_ = nullptr;
    Serial.println("[RTOS] task create failed: TaskStorageScan");
  }
  if (xTaskCreatePinnedToCore(taskWebThunk, "TaskWebControl", 4096, this, 2, &taskWeb_, 0) != pdPASS) {
    ++metrics_.taskCreateFail;
    taskWeb_ = nullptr;
    Serial.println("[RTOS] task create failed: TaskWebControl");
  }
  if (xTaskCreatePinnedToCore(taskUiThunk, "TaskUiOrchestrator", 3072, this, 2, &taskUi_, 1) != pdPASS) {
    ++metrics_.taskCreateFail;
    taskUi_ = nullptr;
    Serial.println("[RTOS] task create failed: TaskUiOrchestrator");
  }
}

void RadioRuntime::taskAudioThunk(void* arg) {
  reinterpret_cast<RadioRuntime*>(arg)->taskAudioLoop();
}

void RadioRuntime::taskStreamThunk(void* arg) {
  reinterpret_cast<RadioRuntime*>(arg)->taskStreamLoop();
}

void RadioRuntime::taskStorageThunk(void* arg) {
  reinterpret_cast<RadioRuntime*>(arg)->taskStorageLoop();
}

void RadioRuntime::taskWebThunk(void* arg) {
  reinterpret_cast<RadioRuntime*>(arg)->taskWebLoop();
}

void RadioRuntime::taskUiThunk(void* arg) {
  reinterpret_cast<RadioRuntime*>(arg)->taskUiLoop();
}

void RadioRuntime::taskAudioLoop() {
  while (true) {
    ++metrics_.audioTicks;
    lastAudioTickMs_ = millis();
    if (wdtEnabled_) {
      esp_task_wdt_reset();
    }
    vTaskDelay(pdMS_TO_TICKS(kTickAudioMs));
  }
}

void RadioRuntime::taskStreamLoop() {
  while (true) {
    const uint32_t nowMs = millis();
    if (wifi_ != nullptr) {
      wifi_->update(nowMs);
    }
    if (radio_ != nullptr) {
      radio_->update(nowMs);
    }
    ++metrics_.streamTicks;
    lastStreamTickMs_ = nowMs;
    if (wdtEnabled_) {
      esp_task_wdt_reset();
    }
    vTaskDelay(pdMS_TO_TICKS(kTickStreamMs));
  }
}

void RadioRuntime::taskStorageLoop() {
  while (true) {
    ++metrics_.storageTicks;
    lastStorageTickMs_ = millis();
    if (wdtEnabled_) {
      esp_task_wdt_reset();
    }
    vTaskDelay(pdMS_TO_TICKS(kTickStorageMs));
  }
}

void RadioRuntime::taskWebLoop() {
  while (true) {
    const uint32_t nowMs = millis();
    if (web_ != nullptr) {
      web_->update(nowMs);
    }
    ++metrics_.webTicks;
    lastWebTickMs_ = nowMs;
    if (wdtEnabled_) {
      esp_task_wdt_reset();
    }
    vTaskDelay(pdMS_TO_TICKS(kTickWebMs));
  }
}

void RadioRuntime::taskUiLoop() {
  Command cmd;
  while (true) {
    while (cmdQueue_ != nullptr && xQueueReceive(cmdQueue_, &cmd, 0U) == pdTRUE) {
      if (cmd.type == CommandType::kScanWifi && wifi_ != nullptr) {
        wifi_->requestScan("runtime_queue");
      }
      if (evtQueue_ != nullptr) {
        xQueueSend(evtQueue_, &cmd, 0U);
        ++metrics_.evtPushed;
      }
    }
    ++metrics_.uiTicks;
    lastUiTickMs_ = millis();
    if (wdtEnabled_) {
      esp_task_wdt_reset();
    }
    vTaskDelay(pdMS_TO_TICKS(kTickUiMs));
  }
}

size_t RadioRuntime::taskSnapshots(TaskSnapshot* out, size_t max) const {
  if (out == nullptr || max == 0U) {
    return 0U;
  }
  size_t count = 0U;
  auto add = [&](const char* name,
                 TaskHandle_t handle,
                 uint32_t ticks,
                 uint32_t lastTickMs,
                 uint8_t core) {
    if (count >= max) {
      return;
    }
    TaskSnapshot& snap = out[count++];
    snap.name = name;
    snap.ticks = ticks;
    snap.lastTickMs = lastTickMs;
    snap.core = core;
    if (handle != nullptr) {
      const UBaseType_t words = uxTaskGetStackHighWaterMark(handle);
      snap.stackMinWords = static_cast<uint32_t>(words);
      snap.stackMinBytes = static_cast<uint32_t>(words * sizeof(StackType_t));
    }
  };
  add("TaskAudioEngine", taskAudio_, metrics_.audioTicks, lastAudioTickMs_, kTaskCoreAudio);
  add("TaskStreamNet", taskStream_, metrics_.streamTicks, lastStreamTickMs_, kTaskCoreNet);
  add("TaskStorageScan", taskStorage_, metrics_.storageTicks, lastStorageTickMs_, kTaskCoreStorage);
  add("TaskWebControl", taskWeb_, metrics_.webTicks, lastWebTickMs_, kTaskCoreWeb);
  add("TaskUiOrchestrator", taskUi_, metrics_.uiTicks, lastUiTickMs_, kTaskCoreUi);
  return count;
}

bool RadioRuntime::enabled() const {
  return enabled_;
}

bool RadioRuntime::started() const {
  return started_;
}
