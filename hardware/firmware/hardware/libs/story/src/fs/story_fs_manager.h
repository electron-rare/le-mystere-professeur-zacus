#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <FS.h>

#include "../core/scenario_def.h"

struct AppConfig {
  char appId[64];
  char appType[32];
  JsonObject params;
};

struct StoryScenarioInfo {
  char id[32] = "";
  uint16_t version = 0U;
  uint32_t estimatedDurationS = 0U;
};

class StoryFsManager {
 public:
  explicit StoryFsManager(const char* story_root = "/story");

  // Mounts the filesystem and validates the Story directory structure.
  bool init();
  void cleanup();

  // Loads a scenario JSON into a cached ScenarioDef snapshot.
  bool loadScenario(const char* scenario_id);

  bool listScenarios(StoryScenarioInfo* out, size_t maxCount, size_t* outCount) const;
  bool fsInfo(uint32_t* totalBytes, uint32_t* usedBytes, uint16_t* scenarioCount) const;

  // Returns cached step/resource data (read-only).
  const StepDef* getStep(const char* step_id) const;
  const ResourceBindings* getResources(const char* step_id) const;
  const AppConfig* getAppConfig(const char* app_id);

  // Verifies checksum for a single resource JSON.
  bool validateChecksum(const char* resource_type, const char* resource_id);
  // Logs resources under /story/<type> to Serial.
  void listResources(const char* resource_type);

  const ScenarioDef* scenario() const;

 private:
  static constexpr size_t kMaxSteps = 12U;
  static constexpr size_t kAppConfigCacheCount = 4U;
  static constexpr size_t kStringPoolSize = 2048U;

  struct StepRuntime {
    TransitionDef transitions[12] = {};
    const char* actionIds[8] = {};
    const char* appIds[6] = {};
  };

  struct AppConfigCache {
    bool valid = false;
    char appId[64] = {};
    char appType[32] = {};
    StaticJsonDocument<256> doc = {};
    JsonObject params = JsonObject();
    AppConfig appConfig = {};
  };

  bool loadJson(const char* path, DynamicJsonDocument& doc);
  bool verifyChecksum(const char* resource_path);
  bool ensureBuffers();
  bool ensureStoryDirs();
  bool loadScenarioInfoFromFile(const char* path, StoryScenarioInfo* out) const;
  bool parseScenarioJson(fs::File& file, StoryScenarioInfo* out) const;
  bool loadEntityJson(const char* entityType, const char* entityId);
  const char* buildResourcePath(const char* resource_type,
                                const char* resource_id,
                                const char* extension,
                                char* out,
                                size_t out_len) const;
  const char* storeString(const char* value);
  void resetScenarioData();

  char storyRoot_[32] = {};
  bool initialized_ = false;
  ScenarioDef scenario_ = {};
  StepDef* steps_ = nullptr;
  StepRuntime* stepRuntime_ = nullptr;
  uint8_t stepCount_ = 0U;

  char* stringPool_ = nullptr;
  size_t stringOffset_ = 0U;
  size_t stringCapacity_ = 0U;

  AppConfigCache* appConfigs_ = nullptr;
};
