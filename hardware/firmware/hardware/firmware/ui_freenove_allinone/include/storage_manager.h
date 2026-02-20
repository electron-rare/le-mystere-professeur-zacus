// storage_manager.h - LittleFS helpers and fallback provisioning.
#pragma once

#include <Arduino.h>

class StorageManager {
 public:
  bool begin();
  bool ensurePath(const char* path);
  bool fileExists(const char* path) const;
  String loadTextFile(const char* path) const;
  String loadScenePayloadById(const char* scene_id) const;
  String resolveAudioPathByPackId(const char* pack_id) const;
  bool ensureDefaultScenarioFile(const char* path);
  uint32_t checksum(const char* path) const;
};
