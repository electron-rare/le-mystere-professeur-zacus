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
  bool hasSdCard() const;
  bool syncStoryFileFromSd(const char* story_path);
  bool syncStoryTreeFromSd();
  bool ensureDefaultStoryBundle();
  bool ensureDefaultScenarioFile(const char* path);
  uint32_t checksum(const char* path) const;

 private:
  bool mountSdCard();
  bool readTextFileWithOrigin(const char* path, String* out_payload, String* out_origin) const;
  bool readTextFromLittleFs(const char* path, String* out_payload) const;
  bool readTextFromSdCard(const char* path, String* out_payload) const;
  String normalizeAbsolutePath(const char* path) const;
  String stripSdPrefix(const char* path) const;
  bool pathExistsOnLittleFs(const char* path) const;
  bool pathExistsOnSdCard(const char* path) const;
  bool ensureParentDirectoriesOnLittleFs(const char* file_path) const;
  bool writeTextToLittleFs(const char* path, const char* payload) const;
  bool provisionEmbeddedAsset(const char* path, const char* payload, bool* out_written = nullptr) const;
  bool copyFileFromSdToLittleFs(const char* src_path, const char* dst_path) const;
  bool copyStoryDirectoryFromSd(const char* relative_dir);
  String resolveReadableAssetPath(const String& absolute_path) const;

  bool sd_ready_ = false;
};
