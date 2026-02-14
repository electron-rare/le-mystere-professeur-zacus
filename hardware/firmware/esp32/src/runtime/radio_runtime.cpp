#include "radio_runtime.h"

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
  xTaskCreatePinnedToCore(taskAudioThunk, "TaskAudioEngine", 3072, this, 4, &taskAudio_, 1);
  xTaskCreatePinnedToCore(taskStreamThunk, "TaskStreamNet", 4096, this, 3, &taskStream_, 0);
  xTaskCreatePinnedToCore(taskStorageThunk, "TaskStorageScan", 3072, this, 2, &taskStorage_, 0);
  xTaskCreatePinnedToCore(taskWebThunk, "TaskWebControl", 4096, this, 2, &taskWeb_, 0);
  xTaskCreatePinnedToCore(taskUiThunk, "TaskUiOrchestrator", 3072, this, 2, &taskUi_, 1);
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
    vTaskDelay(pdMS_TO_TICKS(kTickStreamMs));
  }
}

void RadioRuntime::taskStorageLoop() {
  while (true) {
    ++metrics_.storageTicks;
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
    vTaskDelay(pdMS_TO_TICKS(kTickUiMs));
  }
}
