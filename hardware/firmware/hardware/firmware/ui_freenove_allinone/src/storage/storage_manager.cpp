// storage_manager.cpp - LittleFS + SD story provisioning helpers.
#include "storage_manager.h"

#include <ArduinoJson.h>
#include <FS.h>
#include <LittleFS.h>

#if defined(ARDUINO_ARCH_ESP32) && __has_include(<SD_MMC.h>)
#include <SD_MMC.h>
#include "ui_freenove_config.h"
#define ZACUS_HAS_SD_MMC 1
#else
#define ZACUS_HAS_SD_MMC 0
#endif

#include <cstring>
#include <cctype>

#include "resources/screen_scene_registry.h"
#include "scenarios/default_scenario_v2.h"
#include "system/runtime_metrics.h"

namespace {

constexpr const char* kRequiredDirectories[] = {
    "/data",
    "/picture",
    "/music",
    "/audio",
    "/recorder",
    "/story",
    "/story/scenarios",
    "/story/screens",
    "/story/audio",
    "/story/apps",
    "/story/actions",
    "/scenarios",
    "/scenarios/data",
    "/screens",
};

struct EmbeddedStoryAsset {
  const char* path;
  const char* payload;
};

constexpr EmbeddedStoryAsset kEmbeddedStoryAssets[] = {
    {"/story/apps/APP_WIFI.json", R"JSON({"id":"APP_WIFI","app":"WIFI_STACK","config":{"hostname":"zacus-freenove","ap_policy":"if_no_known_wifi","pause_local_retry_when_ap_client":true,"local_retry_ms":15000,"ap_default_ssid":"Freenove-Setup"}})JSON"},
    {"/story/scenarios/DEFAULT.json", R"JSON({"scenario":"DEFAULT","source":"embedded_minimal"})JSON"},
};


uint32_t fnv1aUpdate(uint32_t hash, uint8_t value) {
  hash ^= value;
  hash *= 16777619UL;
  return hash;
}

bool ensureParentDirectories(fs::FS& file_system, const char* file_path) {
  if (file_path == nullptr || file_path[0] != '/') {
    return false;
  }

  String parent_path = file_path;
  const int last_slash = parent_path.lastIndexOf('/');
  if (last_slash <= 0) {
    return true;
  }
  parent_path = parent_path.substring(0, static_cast<unsigned int>(last_slash));
  if (parent_path.isEmpty()) {
    return true;
  }

  int segment_start = 1;
  String current_path;
  while (segment_start < static_cast<int>(parent_path.length())) {
    const int next_slash = parent_path.indexOf('/', segment_start);
    const int segment_end = (next_slash < 0) ? static_cast<int>(parent_path.length()) : next_slash;
    if (segment_end <= segment_start) {
      break;
    }
    current_path += "/";
    current_path += parent_path.substring(segment_start, static_cast<unsigned int>(segment_end));
    if (!file_system.exists(current_path.c_str()) && !file_system.mkdir(current_path.c_str())) {
      return false;
    }
    if (next_slash < 0) {
      break;
    }
    segment_start = next_slash + 1;
  }
  return true;
}

String normalizeAssetPath(const char* raw_path) {
  if (raw_path == nullptr || raw_path[0] == '\0') {
    return String();
  }
  String normalized = raw_path;
  if (!normalized.startsWith("/")) {
    normalized = "/" + normalized;
  }
  return normalized;
}

String sceneIdToSlug(const char* scene_id) {
  if (scene_id == nullptr || scene_id[0] == '\0') {
    return String();
  }
  String slug = scene_id;
  if (slug.startsWith("SCENE_")) {
    slug = slug.substring(6);
  }
  slug.toLowerCase();
  return slug;
}

String packIdToSlug(const char* pack_id) {
  if (pack_id == nullptr || pack_id[0] == '\0') {
    return String();
  }
  String slug = pack_id;
  if (slug.startsWith("PACK_")) {
    slug = slug.substring(5);
  }
  slug.toLowerCase();
  return slug;
}

bool startsWithIgnoreCase(const char* text, const char* prefix) {
  if (text == nullptr || prefix == nullptr) {
    return false;
  }
  for (size_t index = 0U;; ++index) {
    const char lhs = text[index];
    const char rhs = prefix[index];
    if (rhs == '\0') {
      return true;
    }
    if (lhs == '\0') {
      return false;
    }
    const char lhs_lower = static_cast<char>(std::tolower(static_cast<unsigned char>(lhs)));
    const char rhs_lower = static_cast<char>(std::tolower(static_cast<unsigned char>(rhs)));
    if (lhs_lower != rhs_lower) {
      return false;
    }
  }
}

}  // namespace

bool StorageManager::begin() {
  if (!LittleFS.begin()) {
    Serial.println("[FS] LittleFS mount failed");
    return false;
  }

  for (const char* path : kRequiredDirectories) {
    ensurePath(path);
  }
  sd_ready_ = mountSdCard();
  Serial.printf("[FS] LittleFS ready (sd=%u)\n", sd_ready_ ? 1U : 0U);
  return true;
}

bool StorageManager::mountSdCard() {
#if ZACUS_HAS_SD_MMC
  SD_MMC.end();
  SD_MMC.setPins(FREENOVE_SDMMC_CLK, FREENOVE_SDMMC_CMD, FREENOVE_SDMMC_D0);
  if (!SD_MMC.begin("/sdcard", true)) {
    Serial.println("[FS] SD_MMC unavailable");
    RuntimeMetrics::instance().noteSdError();
    return false;
  }
  const uint8_t card_type = SD_MMC.cardType();
  if (card_type == CARD_NONE) {
    SD_MMC.end();
    Serial.println("[FS] SD_MMC card not detected");
    RuntimeMetrics::instance().noteSdError();
    return false;
  }
  Serial.printf("[FS] SD_MMC mounted size=%lluMB\n",
                static_cast<unsigned long long>(SD_MMC.cardSize() / (1024ULL * 1024ULL)));
  return true;
#else
  return false;
#endif
}

bool StorageManager::ensurePath(const char* path) {
  if (path == nullptr || path[0] == '\0') {
    return false;
  }
  if (LittleFS.exists(path)) {
    return true;
  }
  if (!LittleFS.mkdir(path)) {
    Serial.printf("[FS] mkdir failed: %s\n", path);
    return false;
  }
  Serial.printf("[FS] mkdir: %s\n", path);
  return true;
}

String StorageManager::normalizeAbsolutePath(const char* path) const {
  if (path == nullptr || path[0] == '\0') {
    return String();
  }
  String normalized = path;
  normalized.trim();
  if (normalized.isEmpty()) {
    return String();
  }
  if (!normalized.startsWith("/")) {
    normalized = "/" + normalized;
  }
  return normalized;
}

String StorageManager::stripSdPrefix(const char* path) const {
  String normalized = normalizeAbsolutePath(path);
  if (normalized.isEmpty()) {
    return normalized;
  }
  if (startsWithIgnoreCase(normalized.c_str(), "/sd/")) {
    return normalized.substring(3);
  }
  if (startsWithIgnoreCase(normalized.c_str(), "/sd")) {
    return "/";
  }
  return normalized;
}

bool StorageManager::pathExistsOnLittleFs(const char* path) const {
  const String normalized = normalizeAbsolutePath(path);
  if (normalized.isEmpty()) {
    return false;
  }
  return LittleFS.exists(normalized.c_str());
}

bool StorageManager::pathExistsOnSdCard(const char* path) const {
  if (!sd_ready_) {
    return false;
  }
#if ZACUS_HAS_SD_MMC
  const String sd_path = stripSdPrefix(path);
  if (sd_path.isEmpty()) {
    return false;
  }
  return SD_MMC.exists(sd_path.c_str());
#else
  (void)path;
  return false;
#endif
}

bool StorageManager::fileExists(const char* path) const {
  const String normalized = normalizeAbsolutePath(path);
  if (normalized.isEmpty()) {
    return false;
  }
  if (startsWithIgnoreCase(normalized.c_str(), "/sd/")) {
    return pathExistsOnSdCard(normalized.c_str());
  }
  return pathExistsOnLittleFs(normalized.c_str()) || pathExistsOnSdCard(normalized.c_str());
}

bool StorageManager::readTextFromLittleFs(const char* path, String* out_payload) const {
  if (out_payload == nullptr || !pathExistsOnLittleFs(path)) {
    return false;
  }
  File file = LittleFS.open(path, "r");
  if (!file) {
    return false;
  }
  out_payload->remove(0);
  out_payload->reserve(static_cast<size_t>(file.size()) + 1U);
  while (file.available()) {
    *out_payload += static_cast<char>(file.read());
  }
  file.close();
  return !out_payload->isEmpty();
}

bool StorageManager::readTextFromSdCard(const char* path, String* out_payload) const {
  if (out_payload == nullptr || !pathExistsOnSdCard(path)) {
    return false;
  }
#if ZACUS_HAS_SD_MMC
  const String sd_path = stripSdPrefix(path);
  File file = SD_MMC.open(sd_path.c_str(), "r");
  if (!file) {
    RuntimeMetrics::instance().noteSdError();
    return false;
  }
  out_payload->remove(0);
  out_payload->reserve(static_cast<size_t>(file.size()) + 1U);
  while (file.available()) {
    *out_payload += static_cast<char>(file.read());
  }
  file.close();
  return !out_payload->isEmpty();
#else
  (void)path;
  return false;
#endif
}

bool StorageManager::readTextFileWithOrigin(const char* path, String* out_payload, String* out_origin) const {
  if (out_payload == nullptr) {
    return false;
  }
  const String normalized = normalizeAbsolutePath(path);
  if (normalized.isEmpty()) {
    return false;
  }
  const bool force_sd = startsWithIgnoreCase(normalized.c_str(), "/sd/");
  const bool prefer_sd = !force_sd && startsWithIgnoreCase(normalized.c_str(), "/story/");

  String payload;
  if (force_sd) {
    if (!readTextFromSdCard(normalized.c_str(), &payload)) {
      return false;
    }
    *out_payload = payload;
    if (out_origin != nullptr) {
      *out_origin = stripSdPrefix(normalized.c_str());
      *out_origin = "/sd" + *out_origin;
    }
    return true;
  }

  if (prefer_sd && readTextFromSdCard(normalized.c_str(), &payload)) {
    *out_payload = payload;
    if (out_origin != nullptr) {
      *out_origin = "/sd" + stripSdPrefix(normalized.c_str());
    }
    return true;
  }
  if (readTextFromLittleFs(normalized.c_str(), &payload)) {
    *out_payload = payload;
    if (out_origin != nullptr) {
      *out_origin = normalized;
    }
    return true;
  }
  if (readTextFromSdCard(normalized.c_str(), &payload)) {
    *out_payload = payload;
    if (out_origin != nullptr) {
      *out_origin = "/sd" + stripSdPrefix(normalized.c_str());
    }
    return true;
  }
  return false;
}

String StorageManager::loadTextFile(const char* path) const {
  String payload;
  String origin;
  if (!readTextFileWithOrigin(path, &payload, &origin)) {
    return String();
  }
  return payload;
}

String StorageManager::resolveReadableAssetPath(const String& absolute_path) const {
  if (absolute_path.isEmpty()) {
    return String();
  }
  if (startsWithIgnoreCase(absolute_path.c_str(), "/sd/")) {
    return pathExistsOnSdCard(absolute_path.c_str()) ? absolute_path : String();
  }
  if (pathExistsOnLittleFs(absolute_path.c_str())) {
    return absolute_path;
  }
  if (pathExistsOnSdCard(absolute_path.c_str())) {
    return "/sd" + absolute_path;
  }
  return String();
}

String StorageManager::loadScenePayloadById(const char* scene_id) const {
  if (scene_id == nullptr || scene_id[0] == '\0') {
    return String();
  }

  const char* normalized_scene_id = storyNormalizeScreenSceneId(scene_id);
  if (normalized_scene_id == nullptr) {
    Serial.printf("[FS] scene payload missing for unknown scene id=%s\n", scene_id);
    return String();
  }
  if (std::strcmp(scene_id, normalized_scene_id) != 0) {
    Serial.printf("[FS] scene payload alias normalized: %s -> %s\n", scene_id, normalized_scene_id);
  }

  const String id = normalized_scene_id;
  const String raw_id = scene_id;
  String candidates[14];
  size_t candidate_count = 0U;
  auto add_candidate = [&candidate_count, &candidates](const String& value) {
    for (size_t index = 0U; index < candidate_count; ++index) {
      if (candidates[index] == value) {
        return;
      }
    }
    if (candidate_count < (sizeof(candidates) / sizeof(candidates[0]))) {
      candidates[candidate_count] = value;
      ++candidate_count;
    }
  };

  auto add_scene_candidates = [&add_candidate](const String& scene_name) {
    const String scene_slug = sceneIdToSlug(scene_name.c_str());
    add_candidate("/story/screens/" + scene_name + ".json");
    add_candidate("/story/screens/" + scene_slug + ".json");
    add_candidate("/screens/" + scene_name + ".json");
    add_candidate("/screens/" + scene_slug + ".json");
    add_candidate("/scenarios/data/scene_" + scene_slug + ".json");
    add_candidate("/sd/story/screens/" + scene_name + ".json");
    add_candidate("/sd/story/screens/" + scene_slug + ".json");
  };

  add_scene_candidates(id);
  if (raw_id != id) {
    // Keep alias candidates during migration to tolerate legacy payload names.
    add_scene_candidates(raw_id);
  }
  for (size_t index = 0U; index < candidate_count; ++index) {
    const String& candidate = candidates[index];
    String payload;
    String origin;
    if (!readTextFileWithOrigin(candidate.c_str(), &payload, &origin)) {
      continue;
    }
    if (raw_id != id && candidate.indexOf(raw_id) >= 0) {
      Serial.printf("[FS] scene payload loaded from legacy alias path: %s\n", candidate.c_str());
    }
    Serial.printf("[FS] scene %s -> %s (id=%s)\n", scene_id, origin.c_str(), normalized_scene_id);
    return payload;
  }

  Serial.printf("[FS] scene payload missing for id=%s (normalized=%s)\n", scene_id, normalized_scene_id);
  return String();
}

String StorageManager::resolveAudioPathByPackId(const char* pack_id) const {
  if (pack_id == nullptr || pack_id[0] == '\0') {
    return String();
  }

  const String id = pack_id;
  const String slug = packIdToSlug(pack_id);
  const String json_candidates[] = {
      "/story/audio/" + id + ".json",
      "/story/audio/" + slug + ".json",
      "/audio/" + id + ".json",
      "/audio/" + slug + ".json",
      "/sd/story/audio/" + id + ".json",
      "/sd/story/audio/" + slug + ".json",
  };

  for (const String& json_path : json_candidates) {
    String payload;
    String origin;
    if (!readTextFileWithOrigin(json_path.c_str(), &payload, &origin) || payload.isEmpty()) {
      continue;
    }

    StaticJsonDocument<384> document;
    const DeserializationError error = deserializeJson(document, payload);
    if (error) {
      Serial.printf("[FS] invalid audio pack json %s (%s)\n", origin.c_str(), error.c_str());
      continue;
    }

    const char* file_path = document["file"] | document["path"] | document["asset"];
    if (file_path == nullptr || file_path[0] == '\0') {
      file_path = document["content"]["file"] | document["content"]["path"] | document["content"]["asset"];
    }

    const char* asset_id = "";
    if (file_path == nullptr || file_path[0] == '\0') {
      asset_id = document["asset_id"] | "";
      if (asset_id[0] == '\0') {
        asset_id = document["assetId"] | "";
      }
      if (asset_id[0] == '\0') {
        asset_id = document["content"]["asset_id"] | "";
      }
      if (asset_id[0] == '\0') {
        asset_id = document["content"]["assetId"] | "";
      }
      if (asset_id[0] != '\0') {
        const String asset_name = asset_id;
        const String asset_candidates[] = {
            "/music/" + asset_name,
            "/audio/" + asset_name,
            "/music/" + asset_name + ".mp3",
            "/audio/" + asset_name + ".mp3",
            "/music/" + asset_name + ".wav",
            "/audio/" + asset_name + ".wav",
        };
        for (const String& asset_candidate : asset_candidates) {
          const String resolved = resolveReadableAssetPath(asset_candidate);
          if (resolved.isEmpty()) {
            continue;
          }
          Serial.printf("[FS] audio pack %s asset_id -> %s (%s)\n",
                        pack_id,
                        resolved.c_str(),
                        origin.c_str());
          return resolved;
        }
      }
      Serial.printf("[FS] audio pack missing file/path: %s\n", origin.c_str());
      continue;
    }

    const String normalized = normalizeAssetPath(file_path);
    const String resolved = resolveReadableAssetPath(normalized);
    if (resolved.isEmpty()) {
      Serial.printf("[FS] audio pack path missing on storage: %s (%s)\n", normalized.c_str(), origin.c_str());
      continue;
    }
    Serial.printf("[FS] audio pack %s -> %s (%s)\n", pack_id, resolved.c_str(), origin.c_str());
    return resolved;
  }

  const String direct_candidates[] = {
      "/music/" + id + ".mp3",
      "/music/" + id + ".wav",
      "/audio/" + id + ".mp3",
      "/audio/" + id + ".wav",
      "/music/" + slug + ".mp3",
      "/music/" + slug + ".wav",
      "/audio/" + slug + ".mp3",
      "/audio/" + slug + ".wav",
  };
  for (const String& candidate : direct_candidates) {
    const String resolved = resolveReadableAssetPath(candidate);
    if (resolved.isEmpty()) {
      continue;
    }
    Serial.printf("[FS] audio pack %s fallback direct=%s\n", pack_id, resolved.c_str());
    return resolved;
  }

  return String();
}

bool StorageManager::ensureParentDirectoriesOnLittleFs(const char* file_path) const {
  return ensureParentDirectories(LittleFS, file_path);
}

bool StorageManager::writeTextToLittleFs(const char* path, const char* payload) const {
  if (path == nullptr || payload == nullptr || path[0] != '/') {
    return false;
  }
  if (!ensureParentDirectoriesOnLittleFs(path)) {
    return false;
  }
  File file = LittleFS.open(path, "w");
  if (!file) {
    return false;
  }
  const size_t written = file.print(payload);
  file.close();
  return written > 0U;
}

bool StorageManager::copyFileFromSdToLittleFs(const char* src_path, const char* dst_path) const {
  if (!sd_ready_ || src_path == nullptr || dst_path == nullptr || src_path[0] != '/' || dst_path[0] != '/') {
    return false;
  }
#if ZACUS_HAS_SD_MMC
  if (!pathExistsOnSdCard(src_path)) {
    RuntimeMetrics::instance().noteSdError();
    return false;
  }
  const String sd_path = stripSdPrefix(src_path);
  File src = SD_MMC.open(sd_path.c_str(), "r");
  if (!src) {
    RuntimeMetrics::instance().noteSdError();
    return false;
  }
  if (!ensureParentDirectoriesOnLittleFs(dst_path)) {
    src.close();
    return false;
  }
  File dst = LittleFS.open(dst_path, "w");
  if (!dst) {
    src.close();
    RuntimeMetrics::instance().noteSdError();
    return false;
  }
  uint8_t buffer[512];
  while (src.available()) {
    const size_t read_bytes = src.read(buffer, sizeof(buffer));
    if (read_bytes == 0U) {
      break;
    }
    if (dst.write(buffer, read_bytes) != read_bytes) {
      dst.close();
      src.close();
      RuntimeMetrics::instance().noteSdError();
      return false;
    }
  }
  dst.close();
  src.close();
  return true;
#else
  (void)src_path;
  (void)dst_path;
  return false;
#endif
}

bool StorageManager::syncStoryFileFromSd(const char* story_path) {
  if (!sd_ready_ || story_path == nullptr || story_path[0] == '\0') {
    return false;
  }
  const String normalized = normalizeAbsolutePath(story_path);
  if (normalized.isEmpty() || !pathExistsOnSdCard(normalized.c_str())) {
    return false;
  }
  const bool copied = copyFileFromSdToLittleFs(normalized.c_str(), normalized.c_str());
  if (copied) {
    Serial.printf("[FS] synced story file from SD: %s\n", normalized.c_str());
  }
  return copied;
}

bool StorageManager::copyStoryDirectoryFromSd(const char* relative_dir) {
  if (!sd_ready_ || relative_dir == nullptr || relative_dir[0] == '\0') {
    return false;
  }
#if ZACUS_HAS_SD_MMC
  String source_dir = "/story/";
  source_dir += relative_dir;
  if (!pathExistsOnSdCard(source_dir.c_str())) {
    return false;
  }

  File dir = SD_MMC.open(source_dir.c_str());
  if (!dir || !dir.isDirectory()) {
    return false;
  }

  bool copied_any = false;
  File entry = dir.openNextFile();
  while (entry) {
    if (!entry.isDirectory()) {
      String src_path = entry.name();
      if (!src_path.isEmpty()) {
        if (copyFileFromSdToLittleFs(src_path.c_str(), src_path.c_str())) {
          copied_any = true;
        }
      }
    }
    entry.close();
    entry = dir.openNextFile();
  }
  return copied_any;
#else
  (void)relative_dir;
  return false;
#endif
}

bool StorageManager::provisionEmbeddedAsset(const char* path,
                                            const char* payload,
                                            bool* out_written) const {
  if (out_written != nullptr) {
    *out_written = false;
  }
  if (path == nullptr || path[0] == '\0' || payload == nullptr) {
    return false;
  }
  if (pathExistsOnLittleFs(path)) {
    return true;
  }
  if (!writeTextToLittleFs(path, payload)) {
    return false;
  }
  if (out_written != nullptr) {
    *out_written = true;
  }
  return true;
}

bool StorageManager::syncStoryTreeFromSd() {
  if (!sd_ready_) {
    return false;
  }
  const char* story_dirs[] = {"scenarios", "screens", "audio", "apps", "actions"};
  bool copied_any = false;
  for (const char* relative_dir : story_dirs) {
    copied_any = copyStoryDirectoryFromSd(relative_dir) || copied_any;
  }
  if (copied_any) {
    Serial.println("[FS] story tree refreshed from SD");
  }
  return copied_any;
}

bool StorageManager::ensureDefaultStoryBundle() {
  uint16_t written_count = 0U;
  for (const EmbeddedStoryAsset& asset : kEmbeddedStoryAssets) {
    bool written = false;
    if (provisionEmbeddedAsset(asset.path, asset.payload, &written) && written) {
      ++written_count;
    }
  }
  if (written_count > 0U) {
    Serial.printf("[FS] provisioned embedded story assets: %u\n", written_count);
  } else if (!pathExistsOnLittleFs("/story/screens/SCENE_LOCKED.json")) {
    Serial.println("[FS] story bundle not embedded; run buildfs/uploadfs for full content");
  }
  return true;
}

bool StorageManager::ensureDefaultScenarioFile(const char* path) {
  const String normalized = normalizeAbsolutePath(path);
  if (normalized.isEmpty()) {
    return false;
  }
  if (pathExistsOnLittleFs(normalized.c_str())) {
    return true;
  }
  if (syncStoryFileFromSd(normalized.c_str())) {
    return true;
  }

  const ScenarioDef* scenario = storyScenarioV2Default();
  if (scenario == nullptr) {
    Serial.println("[FS] built-in scenario unavailable");
    return false;
  }

  StaticJsonDocument<256> document;
  document["scenario"] = (scenario->id != nullptr) ? scenario->id : "DEFAULT";
  document["source"] = "auto-fallback";
  document["version"] = scenario->version;
  document["step_count"] = scenario->stepCount;
  String payload;
  serializeJson(document, payload);
  payload += "\n";
  if (!writeTextToLittleFs(normalized.c_str(), payload.c_str())) {
    Serial.printf("[FS] cannot create default scenario file: %s\n", normalized.c_str());
    return false;
  }
  Serial.printf("[FS] default scenario provisioned: %s\n", normalized.c_str());
  return true;
}

bool StorageManager::hasSdCard() const {
  return sd_ready_;
}

uint32_t StorageManager::checksum(const char* path) const {
  const String normalized = normalizeAbsolutePath(path);
  if (normalized.isEmpty()) {
    return 0U;
  }

  File file;
  if (pathExistsOnLittleFs(normalized.c_str())) {
    file = LittleFS.open(normalized.c_str(), "r");
  } else if (pathExistsOnSdCard(normalized.c_str())) {
#if ZACUS_HAS_SD_MMC
    file = SD_MMC.open(stripSdPrefix(normalized.c_str()).c_str(), "r");
#endif
  }
  if (!file) {
    return 0U;
  }

  uint32_t hash = 2166136261UL;
  while (file.available()) {
    const int value = file.read();
    if (value < 0) {
      break;
    }
    hash = fnv1aUpdate(hash, static_cast<uint8_t>(value));
  }
  file.close();
  return hash;
}
