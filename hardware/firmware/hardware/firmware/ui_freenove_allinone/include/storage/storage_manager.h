// storage_manager.h - LittleFS helpers and fallback provisioning.
#pragma once

#include <Arduino.h>

class StorageManager {
 public:
  StorageManager() = default;
  ~StorageManager() = default;
  StorageManager(const StorageManager&) = delete;
  StorageManager& operator=(const StorageManager&) = delete;
  StorageManager(StorageManager&&) = delete;
  StorageManager& operator=(StorageManager&&) = delete;

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
  void invalidateStoryCaches() const;
  bool isStoryScreenPayloadPresent() const;
  void noteSdAccessFailure(const char* operation, const char* path, int error_code) const;
  void noteSdAccessSuccess() const;

  mutable bool sd_ready_ = false;
  mutable uint8_t sd_failure_streak_ = 0U;
  static constexpr uint8_t kSceneCacheSlots = 3U;
  static constexpr uint8_t kAudioCacheSlots = 3U;
  mutable String scene_cache_ids_[kSceneCacheSlots];
  mutable String scene_cache_payloads_[kSceneCacheSlots];
  mutable uint8_t scene_cache_next_slot_ = 0U;
  mutable String audio_cache_pack_ids_[kAudioCacheSlots];
  mutable String audio_cache_paths_[kAudioCacheSlots];
  mutable uint8_t audio_cache_next_slot_ = 0U;
};
