#include "zacus_story_portable/story_portable_runtime.h"

#include <cstdio>
#include <cstring>

#include <ArduinoJson.h>
#include <FS.h>

#include "core/scenario_def.h"
#include "fs/story_fs_manager.h"
#include "generated/scenarios_gen.h"
#include "zacus_story_portable/tinyfsm_core.h"

class StoryControllerV2 {
 public:
  struct StoryControllerV2Snapshot {
    bool enabled = false;
    bool running = false;
    bool paused = false;
    const char* scenarioId = nullptr;
    const char* stepId = nullptr;
    bool mp3GateOpen = true;
    uint8_t queueDepth = 0U;
    const char* appHostError = "OK";
    const char* engineError = "OK";
    uint32_t etape2DueMs = 0U;
    bool testMode = false;
  };

  bool begin(uint32_t nowMs);
  bool setScenario(const char* scenarioId, uint32_t nowMs, const char* source);
  bool setScenarioFromDefinition(const ScenarioDef& scenario, uint32_t nowMs, const char* source);
  void reset(uint32_t nowMs, const char* source);
  void update(uint32_t nowMs);
  bool jumpToStep(const char* stepId, uint32_t nowMs, const char* source);
  bool postSerialEvent(const char* eventName, uint32_t nowMs, const char* source);
  void printScenarioList(const char* source) const;
  bool validateActiveScenario(const char* source) const;
  StoryControllerV2Snapshot snapshot(bool enabled, uint32_t nowMs) const;
  const char* lastError() const;
};

namespace {

constexpr size_t kCatalogMax = 24U;

void copyText(char* out, size_t outLen, const char* value) {
  if (out == nullptr || outLen == 0U) {
    return;
  }
  out[0] = '\0';
  if (value == nullptr || value[0] == '\0') {
    return;
  }
  snprintf(out, outLen, "%s", value);
}

bool isOkCode(const char* code) {
  return code != nullptr && strcmp(code, "OK") == 0;
}

class BeginEvent final : public zacus_story_portable::tinyfsm::Event {
 public:
  explicit BeginEvent(uint32_t now) : Event(Event::Kind::kBegin), nowMs(now) {}
  const uint32_t nowMs;
  mutable bool ok = false;
};

class LoadEvent final : public zacus_story_portable::tinyfsm::Event {
 public:
  LoadEvent(const char* id, uint32_t now, const char* src)
      : Event(Event::Kind::kLoad), scenarioId(id), nowMs(now), source(src) {}

  const char* scenarioId;
  const uint32_t nowMs;
  const char* source;
  mutable bool ok = false;
};

class UpdateEvent final : public zacus_story_portable::tinyfsm::Event {
 public:
  explicit UpdateEvent(uint32_t now) : Event(Event::Kind::kUpdate), nowMs(now) {}
  const uint32_t nowMs;
};

class StopEvent final : public zacus_story_portable::tinyfsm::Event {
 public:
  StopEvent(uint32_t now, const char* src)
      : Event(Event::Kind::kStop), nowMs(now), source(src) {}
  const uint32_t nowMs;
  const char* source;
};

}  // namespace

bool StoryPortableCatalog::listScenarios(StoryFsManager* fsManager,
                                         StoryPortableCatalogEntry* out,
                                         size_t maxCount,
                                         size_t* outCount) {
  if (outCount != nullptr) {
    *outCount = 0U;
  }
  if (out == nullptr || maxCount == 0U) {
    return false;
  }

  size_t count = 0U;
  auto addOrMerge = [&](const char* id, bool fromFs, bool fromGenerated, uint16_t version, uint32_t durationS) {
    if (id == nullptr || id[0] == '\0') {
      return;
    }
    for (size_t i = 0U; i < count; ++i) {
      if (strcmp(out[i].id, id) == 0) {
        out[i].fromLittleFs = out[i].fromLittleFs || fromFs;
        out[i].fromGenerated = out[i].fromGenerated || fromGenerated;
        if (fromFs) {
          out[i].version = version;
          out[i].estimatedDurationS = durationS;
        }
        return;
      }
    }
    if (count >= maxCount) {
      return;
    }
    copyText(out[count].id, sizeof(out[count].id), id);
    out[count].fromLittleFs = fromFs;
    out[count].fromGenerated = fromGenerated;
    out[count].version = version;
    out[count].estimatedDurationS = durationS;
    ++count;
  };

  if (fsManager != nullptr) {
    StoryScenarioInfo infos[kCatalogMax] = {};
    size_t fsCount = 0U;
    if (fsManager->listScenarios(infos, kCatalogMax, &fsCount)) {
      for (size_t i = 0U; i < fsCount; ++i) {
        addOrMerge(infos[i].id, true, false, infos[i].version, infos[i].estimatedDurationS);
      }
    }
  }

  const uint8_t generatedCount = generatedScenarioCount();
  for (uint8_t i = 0U; i < generatedCount; ++i) {
    const char* id = generatedScenarioIdAt(i);
    addOrMerge(id, false, true, 0U, 0U);
  }

  if (outCount != nullptr) {
    *outCount = count;
  }
  return count > 0U;
}

bool StoryPortableAssets::validateChecksum(StoryFsManager* fsManager,
                                           const char* resourceType,
                                           const char* resourceId) {
  if (fsManager == nullptr) {
    return false;
  }
  return fsManager->validateChecksum(resourceType, resourceId);
}

void StoryPortableAssets::listResources(StoryFsManager* fsManager, const char* resourceType) {
  if (fsManager == nullptr) {
    return;
  }
  fsManager->listResources(resourceType);
}

bool StoryPortableAssets::fsInfo(StoryFsManager* fsManager,
                                 uint32_t* totalBytes,
                                 uint32_t* usedBytes,
                                 uint16_t* scenarioCount) {
  if (fsManager == nullptr) {
    return false;
  }
  return fsManager->fsInfo(totalBytes, usedBytes, scenarioCount);
}

namespace {

class IdleState;
class RunningState;
class ErrorState;

IdleState& idleState();
RunningState& runningState();
ErrorState& errorState();

class IdleState final : public zacus_story_portable::tinyfsm::State<StoryPortableRuntime> {
 public:
  const char* name() const override {
    return "idle";
  }

  void onEvent(StoryPortableRuntime& runtime,
               const zacus_story_portable::tinyfsm::Event& event) override;
};

class RunningState final : public zacus_story_portable::tinyfsm::State<StoryPortableRuntime> {
 public:
  const char* name() const override {
    return "running";
  }

  void onEvent(StoryPortableRuntime& runtime,
               const zacus_story_portable::tinyfsm::Event& event) override;
};

class ErrorState final : public zacus_story_portable::tinyfsm::State<StoryPortableRuntime> {
 public:
  const char* name() const override {
    return "error";
  }

  void onEvent(StoryPortableRuntime& runtime,
               const zacus_story_portable::tinyfsm::Event& event) override;
};

IdleState& idleState() {
  static IdleState state;
  return state;
}

RunningState& runningState() {
  static RunningState state;
  return state;
}

ErrorState& errorState() {
  static ErrorState state;
  return state;
}

bool runtimeApplyScenario(StoryPortableRuntime& runtime,
                          const char* scenarioId,
                          uint32_t nowMs,
                          const char* source) {
  return runtime.setScenario(scenarioId, nowMs, source);
}

void moveState(StoryPortableRuntime& runtime, StoryPortableRuntime::RuntimeState state) {
  runtime.setState(state);
}

void IdleState::onEvent(StoryPortableRuntime& runtime,
                        const zacus_story_portable::tinyfsm::Event& event) {
  if (event.kind == zacus_story_portable::tinyfsm::Event::Kind::kBegin) {
    const auto& begin = static_cast<const BeginEvent&>(event);
    LoadEvent load("DEFAULT", begin.nowMs, "story_portable_begin");
    onEvent(runtime, load);
    begin.ok = load.ok;
    return;
  }

  if (event.kind == zacus_story_portable::tinyfsm::Event::Kind::kLoad) {
    const auto& load = static_cast<const LoadEvent&>(event);
    const bool ok = runtimeApplyScenario(runtime, load.scenarioId, load.nowMs, load.source);
    load.ok = ok;
    moveState(runtime, ok ? StoryPortableRuntime::RuntimeState::kRunning
                          : StoryPortableRuntime::RuntimeState::kError);
    return;
  }

  if (event.kind == zacus_story_portable::tinyfsm::Event::Kind::kStop) {
    moveState(runtime, StoryPortableRuntime::RuntimeState::kIdle);
  }
}

void RunningState::onEvent(StoryPortableRuntime& runtime,
                           const zacus_story_portable::tinyfsm::Event& event) {
  if (event.kind == zacus_story_portable::tinyfsm::Event::Kind::kLoad) {
    const auto& load = static_cast<const LoadEvent&>(event);
    const bool ok = runtimeApplyScenario(runtime, load.scenarioId, load.nowMs, load.source);
    load.ok = ok;
    moveState(runtime, ok ? StoryPortableRuntime::RuntimeState::kRunning
                          : StoryPortableRuntime::RuntimeState::kError);
    return;
  }

  if (event.kind == zacus_story_portable::tinyfsm::Event::Kind::kUpdate) {
    const auto& update = static_cast<const UpdateEvent&>(event);
    if (runtime.controllerV2() != nullptr) {
      runtime.controllerV2()->update(update.nowMs);
    }
    return;
  }

  if (event.kind == zacus_story_portable::tinyfsm::Event::Kind::kStop) {
    const auto& stop = static_cast<const StopEvent&>(event);
    if (runtime.controllerV2() != nullptr) {
      runtime.controllerV2()->reset(stop.nowMs, stop.source);
    }
    moveState(runtime, StoryPortableRuntime::RuntimeState::kIdle);
    return;
  }

  if (event.kind == zacus_story_portable::tinyfsm::Event::Kind::kBegin) {
    auto& begin = static_cast<const BeginEvent&>(event);
    begin.ok = true;
  }
}

void ErrorState::onEvent(StoryPortableRuntime& runtime,
                         const zacus_story_portable::tinyfsm::Event& event) {
  if (event.kind == zacus_story_portable::tinyfsm::Event::Kind::kLoad) {
    const auto& load = static_cast<const LoadEvent&>(event);
    const bool ok = runtimeApplyScenario(runtime, load.scenarioId, load.nowMs, load.source);
    load.ok = ok;
    moveState(runtime, ok ? StoryPortableRuntime::RuntimeState::kRunning
                          : StoryPortableRuntime::RuntimeState::kError);
    return;
  }

  if (event.kind == zacus_story_portable::tinyfsm::Event::Kind::kBegin) {
    auto& begin = static_cast<const BeginEvent&>(event);
    LoadEvent load("DEFAULT", begin.nowMs, "story_portable_begin");
    onEvent(runtime, load);
    begin.ok = load.ok;
    return;
  }

  if (event.kind == zacus_story_portable::tinyfsm::Event::Kind::kStop) {
    const auto& stop = static_cast<const StopEvent&>(event);
    if (runtime.controllerV2() != nullptr) {
      runtime.controllerV2()->reset(stop.nowMs, stop.source);
    }
    moveState(runtime, StoryPortableRuntime::RuntimeState::kIdle);
  }
}

}  // namespace

void StoryPortableRuntime::configure(const StoryPortableConfig& config) {
  config_ = config;
}

void StoryPortableRuntime::bind(StoryControllerV2* controllerV2, StoryFsManager* fsManager) {
  controllerV2_ = controllerV2;
  fsManager_ = fsManager;
}

bool StoryPortableRuntime::begin(uint32_t nowMs) {
  BeginEvent event(nowMs);
  switch (state_) {
    case RuntimeState::kIdle:
      idleState().onEvent(*this, event);
      break;
    case RuntimeState::kRunning:
      runningState().onEvent(*this, event);
      break;
    case RuntimeState::kError:
    default:
      errorState().onEvent(*this, event);
      break;
  }
  return event.ok;
}

bool StoryPortableRuntime::tryLoadFromLittleFs(const char* scenarioId, uint32_t nowMs, const char* source) {
  if (fsManager_ == nullptr || controllerV2_ == nullptr) {
    return false;
  }
  if (!fsManager_->loadScenario(scenarioId)) {
    return false;
  }
  const ScenarioDef* scenario = fsManager_->scenario();
  if (scenario == nullptr) {
    return false;
  }
  return controllerV2_->setScenarioFromDefinition(*scenario, nowMs, source);
}

bool StoryPortableRuntime::loadScenario(const char* scenarioId, uint32_t nowMs, const char* source) {
  return setScenario(scenarioId, nowMs, source);
}

bool StoryPortableRuntime::setScenario(const char* scenarioId, uint32_t nowMs, const char* source) {
  if (controllerV2_ == nullptr) {
    setError("controller_missing");
    state_ = RuntimeState::kError;
    return false;
  }

  const char* requested = (scenarioId != nullptr && scenarioId[0] != '\0') ? scenarioId : "DEFAULT";

  if (config_.preferLittleFs) {
    if (tryLoadFromLittleFs(requested, nowMs, source)) {
      scenarioFromLittleFs_ = true;
      clearError();
      state_ = RuntimeState::kRunning;
      return true;
    }
    if (config_.strictFsOnly) {
      setError("littlefs_scenario_missing");
      state_ = RuntimeState::kError;
      return false;
    }
  }

  if (!config_.allowGeneratedFallback && config_.preferLittleFs) {
    setError("fallback_disabled");
    state_ = RuntimeState::kError;
    return false;
  }

  if (controllerV2_->setScenario(requested, nowMs, source)) {
    scenarioFromLittleFs_ = false;
    clearError();
    state_ = RuntimeState::kRunning;
    return true;
  }

  setError("scenario_not_found");
  state_ = RuntimeState::kError;
  return false;
}

void StoryPortableRuntime::update(uint32_t nowMs) {
  UpdateEvent event(nowMs);
  switch (state_) {
    case RuntimeState::kIdle:
      idleState().onEvent(*this, event);
      break;
    case RuntimeState::kRunning:
      runningState().onEvent(*this, event);
      break;
    case RuntimeState::kError:
    default:
      errorState().onEvent(*this, event);
      break;
  }
}

void StoryPortableRuntime::stop(uint32_t nowMs, const char* source) {
  StopEvent event(nowMs, source);
  switch (state_) {
    case RuntimeState::kIdle:
      idleState().onEvent(*this, event);
      break;
    case RuntimeState::kRunning:
      runningState().onEvent(*this, event);
      break;
    case RuntimeState::kError:
    default:
      errorState().onEvent(*this, event);
      break;
  }
  if (state_ == RuntimeState::kIdle) {
    clearError();
  }
}

StoryPortableSnapshot StoryPortableRuntime::snapshot(bool enabled, uint32_t nowMs) const {
  StoryPortableSnapshot out = {};
  out.scenarioFromLittleFs = scenarioFromLittleFs_;
  out.lastError = lastError_;
  out.runtimeState = stateLabel();

  if (controllerV2_ == nullptr) {
    return out;
  }

  const StoryControllerV2::StoryControllerV2Snapshot raw = controllerV2_->snapshot(enabled, nowMs);
  out.initialized = raw.enabled || raw.running;
  out.running = raw.running;
  out.mp3GateOpen = raw.mp3GateOpen;
  out.testMode = raw.testMode;
  out.scenarioId = raw.scenarioId;
  out.stepId = raw.stepId;
  if (isOkCode(lastError_)) {
    const char* runtimeError = controllerV2_->lastError();
    if (runtimeError != nullptr && runtimeError[0] != '\0') {
      out.lastError = runtimeError;
    }
  }
  return out;
}

const char* StoryPortableRuntime::lastError() const {
  return lastError_;
}

bool StoryPortableRuntime::scenarioFromLittleFs() const {
  return scenarioFromLittleFs_;
}

const char* StoryPortableRuntime::stateLabel() const {
  switch (state_) {
    case RuntimeState::kIdle:
      return "idle";
    case RuntimeState::kRunning:
      return "running";
    case RuntimeState::kError:
    default:
      return "error";
  }
}

StoryPortableRuntime::RuntimeState StoryPortableRuntime::state() const {
  return state_;
}

void StoryPortableRuntime::setState(RuntimeState state) {
  state_ = state;
}

void StoryPortableRuntime::printScenarioList(const char* source) const {
  StoryPortableCatalogEntry entries[kCatalogMax] = {};
  size_t count = 0U;
  if (!StoryPortableCatalog::listScenarios(fsManager_, entries, kCatalogMax, &count)) {
    if (controllerV2_ != nullptr) {
      controllerV2_->printScenarioList(source);
    }
    return;
  }

  Serial.printf("[STORY_PORTABLE] LIST via=%s count=%u\n",
                source != nullptr ? source : "-",
                static_cast<unsigned int>(count));
  for (size_t i = 0U; i < count; ++i) {
    Serial.printf("[STORY_PORTABLE] LIST[%u] id=%s fs=%u gen=%u version=%u duration_s=%lu\n",
                  static_cast<unsigned int>(i),
                  entries[i].id[0] != '\0' ? entries[i].id : "-",
                  entries[i].fromLittleFs ? 1U : 0U,
                  entries[i].fromGenerated ? 1U : 0U,
                  static_cast<unsigned int>(entries[i].version),
                  static_cast<unsigned long>(entries[i].estimatedDurationS));
  }
}

bool StoryPortableRuntime::validateActiveScenario(const char* source) const {
  if (controllerV2_ == nullptr) {
    return false;
  }
  return controllerV2_->validateActiveScenario(source);
}

bool StoryPortableRuntime::validateChecksum(const char* resourceType, const char* resourceId) const {
  return StoryPortableAssets::validateChecksum(fsManager_, resourceType, resourceId);
}

void StoryPortableRuntime::listResources(const char* resourceType) const {
  StoryPortableAssets::listResources(fsManager_, resourceType);
}

bool StoryPortableRuntime::fsInfo(uint32_t* totalBytes, uint32_t* usedBytes, uint16_t* scenarioCount) const {
  return StoryPortableAssets::fsInfo(fsManager_, totalBytes, usedBytes, scenarioCount);
}

StoryControllerV2* StoryPortableRuntime::controllerV2() const {
  return controllerV2_;
}

StoryFsManager* StoryPortableRuntime::fsManager() const {
  return fsManager_;
}

void StoryPortableRuntime::setError(const char* message) {
  copyText(lastError_, sizeof(lastError_), message);
}

void StoryPortableRuntime::clearError() {
  copyText(lastError_, sizeof(lastError_), "OK");
}
