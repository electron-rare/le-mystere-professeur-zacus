// storage_manager.cpp - LittleFS provisioning helpers.
#include "storage_manager.h"

#include <ArduinoJson.h>
#include <LittleFS.h>

#include "scenarios/default_scenario_v2.h"

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

uint32_t fnv1aUpdate(uint32_t hash, uint8_t value) {
  hash ^= value;
  hash *= 16777619UL;
  return hash;
}

bool ensureParentDirectories(const char* file_path) {
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
    if (!LittleFS.exists(current_path.c_str()) && !LittleFS.mkdir(current_path.c_str())) {
      Serial.printf("[FS] mkdir failed: %s\n", current_path.c_str());
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

}  // namespace

bool StorageManager::begin() {
  if (!LittleFS.begin()) {
    Serial.println("[FS] LittleFS mount failed");
    return false;
  }

  for (const char* path : kRequiredDirectories) {
    ensurePath(path);
  }
  Serial.println("[FS] LittleFS ready");
  return true;
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

bool StorageManager::fileExists(const char* path) const {
  if (path == nullptr || path[0] == '\0') {
    return false;
  }
  return LittleFS.exists(path);
}

String StorageManager::loadTextFile(const char* path) const {
  if (!fileExists(path)) {
    return String();
  }
  File file = LittleFS.open(path, "r");
  if (!file) {
    return String();
  }
  String payload;
  payload.reserve(static_cast<size_t>(file.size()) + 1U);
  while (file.available()) {
    payload += static_cast<char>(file.read());
  }
  file.close();
  return payload;
}

String StorageManager::loadScenePayloadById(const char* scene_id) const {
  if (scene_id == nullptr || scene_id[0] == '\0') {
    return String();
  }

  const String id = scene_id;
  const String slug = sceneIdToSlug(scene_id);
  const String candidates[] = {
      "/story/screens/" + id + ".json",
      "/story/screens/" + slug + ".json",
      "/screens/" + id + ".json",
      "/screens/" + slug + ".json",
      "/scenarios/data/scene_" + slug + ".json",
  };

  for (const String& candidate : candidates) {
    if (!fileExists(candidate.c_str())) {
      continue;
    }
    Serial.printf("[FS] scene %s -> %s\n", scene_id, candidate.c_str());
    return loadTextFile(candidate.c_str());
  }

  Serial.printf("[FS] scene payload missing for id=%s\n", scene_id);
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
  };

  for (const String& json_path : json_candidates) {
    if (!fileExists(json_path.c_str())) {
      continue;
    }

    const String payload = loadTextFile(json_path.c_str());
    if (payload.isEmpty()) {
      continue;
    }

    StaticJsonDocument<384> document;
    const DeserializationError error = deserializeJson(document, payload);
    if (error) {
      Serial.printf("[FS] invalid audio pack json %s (%s)\n", json_path.c_str(), error.c_str());
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
        String asset_name = asset_id;
        const String asset_candidates[] = {
            "/music/" + asset_name,
            "/audio/" + asset_name,
            "/music/" + asset_name + ".mp3",
            "/audio/" + asset_name + ".mp3",
            "/music/" + asset_name + ".wav",
            "/audio/" + asset_name + ".wav",
        };
        for (const String& asset_candidate : asset_candidates) {
          if (fileExists(asset_candidate.c_str())) {
            Serial.printf("[FS] audio pack %s asset_id -> %s (%s)\n",
                          pack_id,
                          asset_candidate.c_str(),
                          json_path.c_str());
            return asset_candidate;
          }
        }
      }
      Serial.printf("[FS] audio pack missing file/path: %s\n", json_path.c_str());
      continue;
    }

    String normalized = normalizeAssetPath(file_path);
    if (normalized.isEmpty()) {
      continue;
    }
    if (!fileExists(normalized.c_str())) {
      Serial.printf("[FS] audio pack path missing on FS: %s (%s)\n", normalized.c_str(), json_path.c_str());
      continue;
    }
    Serial.printf("[FS] audio pack %s -> %s (%s)\n", pack_id, normalized.c_str(), json_path.c_str());
    return normalized;
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
    if (fileExists(candidate.c_str())) {
      Serial.printf("[FS] audio pack %s fallback direct=%s\n", pack_id, candidate.c_str());
      return candidate;
    }
  }

  return String();
}

bool StorageManager::ensureDefaultScenarioFile(const char* path) {
  if (path == nullptr || path[0] == '\0') {
    return false;
  }
  if (fileExists(path)) {
    return true;
  }
  if (!ensureParentDirectories(path)) {
    Serial.printf("[FS] cannot create parent directory for: %s\n", path);
    return false;
  }

  const ScenarioDef* scenario = storyScenarioV2Default();
  if (scenario == nullptr) {
    Serial.println("[FS] built-in scenario unavailable");
    return false;
  }

  File file = LittleFS.open(path, "w");
  if (!file) {
    Serial.printf("[FS] cannot create default scenario file: %s\n", path);
    return false;
  }
  file.printf("{\"scenario\":\"%s\",\"source\":\"auto-fallback\",\"version\":%u,\"step_count\":%u}\n",
              scenario->id != nullptr ? scenario->id : "DEFAULT",
              scenario->version,
              scenario->stepCount);
  file.close();
  Serial.printf("[FS] default scenario provisioned: %s\n", path);
  return true;
}

uint32_t StorageManager::checksum(const char* path) const {
  if (!fileExists(path)) {
    return 0U;
  }
  File file = LittleFS.open(path, "r");
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
