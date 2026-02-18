#pragma once

#include <Arduino.h>
#include <stddef.h>
#include <stdint.h>

class StoryControllerV2;
class StoryFsManager;
struct ScenarioDef;

struct StoryPortableConfig {
  const char* fsRoot = "/story";
  bool preferLittleFs = true;
  bool allowGeneratedFallback = true;
  bool strictFsOnly = false;
};

struct StoryPortableSnapshot {
  bool initialized = false;
  bool scenarioFromLittleFs = false;
  bool running = false;
  bool mp3GateOpen = true;
  bool testMode = false;
  const char* scenarioId = nullptr;
  const char* stepId = nullptr;
  const char* lastError = "OK";
  const char* runtimeState = "idle";
};

struct StoryPortableCatalogEntry {
  char id[32] = {};
  bool fromLittleFs = false;
  bool fromGenerated = false;
  uint16_t version = 0U;
  uint32_t estimatedDurationS = 0U;
};

class StoryPortableCatalog {
 public:
  static bool listScenarios(StoryFsManager* fsManager,
                            StoryPortableCatalogEntry* out,
                            size_t maxCount,
                            size_t* outCount);
};

class StoryPortableAssets {
 public:
  static bool validateChecksum(StoryFsManager* fsManager,
                               const char* resourceType,
                               const char* resourceId);
  static void listResources(StoryFsManager* fsManager, const char* resourceType);
  static bool fsInfo(StoryFsManager* fsManager,
                     uint32_t* totalBytes,
                     uint32_t* usedBytes,
                     uint16_t* scenarioCount);
};

class StoryPortableRuntime {
 public:
  enum class RuntimeState : uint8_t {
    kIdle = 0,
    kRunning,
    kError,
  };

  StoryPortableRuntime() = default;

  void configure(const StoryPortableConfig& config);
  void bind(StoryControllerV2* controllerV2, StoryFsManager* fsManager);

  bool begin(uint32_t nowMs);
  bool loadScenario(const char* scenarioId, uint32_t nowMs, const char* source);
  bool setScenario(const char* scenarioId, uint32_t nowMs, const char* source);
  void update(uint32_t nowMs);
  void stop(uint32_t nowMs, const char* source);

  StoryPortableSnapshot snapshot(bool enabled, uint32_t nowMs) const;
  const char* lastError() const;
  bool scenarioFromLittleFs() const;
  const char* stateLabel() const;
  RuntimeState state() const;
  void setState(RuntimeState state);

  void printScenarioList(const char* source) const;
  bool validateActiveScenario(const char* source) const;
  bool validateChecksum(const char* resourceType, const char* resourceId) const;
  void listResources(const char* resourceType) const;
  bool fsInfo(uint32_t* totalBytes, uint32_t* usedBytes, uint16_t* scenarioCount) const;

  StoryControllerV2* controllerV2() const;
  StoryFsManager* fsManager() const;

 private:
  bool tryLoadFromLittleFs(const char* scenarioId, uint32_t nowMs, const char* source);
  void setError(const char* message);
  void clearError();

  StoryPortableConfig config_ = {};
  StoryControllerV2* controllerV2_ = nullptr;
  StoryFsManager* fsManager_ = nullptr;
  bool scenarioFromLittleFs_ = false;
  char lastError_[48] = "OK";
  RuntimeState state_ = RuntimeState::kIdle;
};
