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

struct EmbeddedStoryAsset {
  const char* path;
  const char* payload;
};

constexpr EmbeddedStoryAsset kEmbeddedStoryAssets[] = {
    {"/story/scenarios/DEFAULT.json",
     R"JSON({"id":"DEFAULT","scenario":"DEFAULT","version":2,"initial_step":"STEP_WAIT_UNLOCK","hardware_events":{"button_short_1":"UNLOCK","button_short_5":"BTN_NEXT","button_long_3":"FORCE_ETAPE2","button_long_4":"FORCE_DONE","espnow_event":"SERIAL:<payload>"},"app_bindings":["APP_AUDIO","APP_SCREEN","APP_GATE","APP_WIFI","APP_ESPNOW"],"actions_catalog":["ACTION_TRACE_STEP","ACTION_FORCE_ETAPE2","ACTION_REFRESH_SD"],"steps":[{"id":"STEP_WAIT_UNLOCK","screen_scene_id":"SCENE_LOCKED"},{"id":"STEP_U_SON_PROTO","screen_scene_id":"SCENE_BROKEN","audio_pack_id":"PACK_BOOT_RADIO"},{"id":"STEP_WAIT_ETAPE2","screen_scene_id":"SCENE_LA_DETECT"},{"id":"STEP_ETAPE2","screen_scene_id":"SCENE_WIN","audio_pack_id":"PACK_WIN"},{"id":"STEP_DONE","screen_scene_id":"SCENE_READY"}],"source":"embedded_default","screen_root":"/story/screens","audio_root":"/story/audio"})JSON"},
    {"/story/apps/APP_AUDIO.json",
     R"JSON({"id":"APP_AUDIO","app":"AUDIO_PACK","config":{"player":"littlefs_mp3","fallback":"builtin_tone","autoplay":true}})JSON"},
    {"/story/apps/APP_ESPNOW.json",
     R"JSON({"id":"APP_ESPNOW","app":"ESPNOW_STACK","config":{"enabled_on_boot":true,"bridge_to_story_event":true,"peers":[],"payload_format":"JSON envelope {msg_id,seq,type,payload,ack} + legacy SC_EVENT formats"}})JSON"},
    {"/story/apps/APP_GATE.json",
     R"JSON({"id":"APP_GATE","app":"MP3_GATE","config":{"mode":"strict","close_on_step_done":true}})JSON"},
    {"/story/apps/APP_LA.json",
     R"JSON({"id":"APP_LA","app":"LA_DETECTOR","config":{"unlock_event":"UNLOCK","timeout_ms":30000}})JSON"},
    {"/story/apps/APP_SCREEN.json",
     R"JSON({"id":"APP_SCREEN","app":"SCREEN_SCENE","config":{"renderer":"lvgl_fx","mode":"effect_first","show_title":false,"show_symbol":true}})JSON"},
    {"/story/apps/APP_WIFI.json",
     R"JSON({"id":"APP_WIFI","app":"WIFI_STACK","config":{"hostname":"zacus-freenove","local_ssid":"Les cils","local_password":"mascarade","ap_policy":"if_no_known_wifi","pause_local_retry_when_ap_client":true,"local_retry_ms":15000,"test_ssid":"Les cils","test_password":"mascarade","ap_default_ssid":"Freenove-Setup","ap_default_password":"mascarade"}})JSON"},
    {"/story/actions/ACTION_FORCE_ETAPE2.json",
     R"JSON({"id":"ACTION_FORCE_ETAPE2","type":"emit_story_event","config":{"event_type":"action","event_name":"ACTION_FORCE_ETAPE2","target":"STEP_ETAPE2"}})JSON"},
    {"/story/actions/ACTION_QUEUE_SONAR.json",
     R"JSON({"id":"ACTION_QUEUE_SONAR","type":"queue_audio_pack","config":{"pack_id":"PACK_SONAR_HINT","priority":"normal"}})JSON"},
    {"/story/actions/ACTION_REFRESH_SD.json",
     R"JSON({"id":"ACTION_REFRESH_SD","type":"refresh_storage","config":{"targets":["story/scenarios","story/screens","story/audio"]}})JSON"},
    {"/story/actions/ACTION_TRACE_STEP.json",
     R"JSON({"id":"ACTION_TRACE_STEP","type":"trace_step","config":{"serial_log":true,"tag":"story_step"}})JSON"},
    {"/story/audio/PACK_BOOT_RADIO.json", R"JSON({"id":"PACK_BOOT_RADIO","file":"/music/boot_radio.mp3","volume":100})JSON"},
    {"/story/audio/PACK_MORSE_HINT.json", R"JSON({"id":"PACK_MORSE_HINT","file":"/music/morse_hint.mp3","volume":100})JSON"},
    {"/story/audio/PACK_SONAR_HINT.json", R"JSON({"id":"PACK_SONAR_HINT","file":"/music/sonar_hint.mp3","volume":100})JSON"},
    {"/story/audio/PACK_WIN.json", R"JSON({"id":"PACK_WIN","file":"/music/win.mp3","volume":100})JSON"},
    {"/story/screens/SCENE_BROKEN.json",
     R"JSON({"id":"SCENE_BROKEN","title":"PROTO U-SON","subtitle":"Signal brouille","symbol":"ALERT","effect":"blink","visual":{"show_title":false,"show_symbol":true,"effect_speed_ms":180},"theme":{"bg":"#2A0508","accent":"#FF4A45","text":"#FFF1F1"},"timeline":{"loop":true,"duration_ms":900,"keyframes":[{"at_ms":0,"effect":"blink","speed_ms":180,"theme":{"bg":"#2A0508","accent":"#FF4A45","text":"#FFF1F1"}},{"at_ms":900,"effect":"scan","speed_ms":520,"theme":{"bg":"#3A0A10","accent":"#FF7873","text":"#FFF7F7"}}]},"transition":{"effect":"glitch","duration_ms":160}})JSON"},
    {"/story/screens/SCENE_LA_DETECT.json",
     R"JSON({"id":"SCENE_LA_DETECT","title":"DETECTION","subtitle":"Balayage en cours","symbol":"SCAN","effect":"scan","visual":{"show_title":false,"show_symbol":true,"effect_speed_ms":960},"theme":{"bg":"#041F1B","accent":"#2CE5A6","text":"#EFFFF8"},"timeline":{"loop":true,"duration_ms":1500,"keyframes":[{"at_ms":0,"effect":"scan","speed_ms":960,"theme":{"bg":"#041F1B","accent":"#2CE5A6","text":"#EFFFF8"}},{"at_ms":1500,"effect":"pulse","speed_ms":620,"theme":{"bg":"#053028","accent":"#62F1C3","text":"#F4FFF9"}}]},"transition":{"effect":"slide_up","duration_ms":220}})JSON"},
    {"/story/screens/SCENE_LOCKED.json",
     R"JSON({"id":"SCENE_LOCKED","title":"VERROUILLE","subtitle":"Attente de debloquage","symbol":"LOCK","effect":"pulse","visual":{"show_title":false,"show_symbol":true,"effect_speed_ms":680},"theme":{"bg":"#08152D","accent":"#3E8DFF","text":"#F5F8FF"},"timeline":{"loop":true,"duration_ms":1400,"keyframes":[{"at_ms":0,"effect":"pulse","speed_ms":680,"theme":{"bg":"#08152D","accent":"#3E8DFF","text":"#F5F8FF"}},{"at_ms":1400,"effect":"blink","speed_ms":420,"theme":{"bg":"#0A1E3A","accent":"#74B0FF","text":"#F8FBFF"}}]},"transition":{"effect":"fade","duration_ms":260}})JSON"},
    {"/story/screens/SCENE_READY.json",
     R"JSON({"id":"SCENE_READY","title":"PRET","subtitle":"Scenario termine","symbol":"READY","effect":"pulse","visual":{"show_title":false,"show_symbol":true,"effect_speed_ms":620},"theme":{"bg":"#0F2A12","accent":"#6CD96B","text":"#EDFFED"},"timeline":{"loop":true,"duration_ms":1600,"keyframes":[{"at_ms":0,"effect":"pulse","speed_ms":620,"theme":{"bg":"#0F2A12","accent":"#6CD96B","text":"#EDFFED"}},{"at_ms":1600,"effect":"none","speed_ms":620,"theme":{"bg":"#133517","accent":"#9EE49D","text":"#F4FFF4"}}]},"transition":{"effect":"fade","duration_ms":220}})JSON"},
    {"/story/screens/SCENE_REWARD.json",
     R"JSON({"id":"SCENE_REWARD","title":"RECOMPENSE","subtitle":"Indice debloque","symbol":"WIN","effect":"celebrate","visual":{"show_title":false,"show_symbol":true,"effect_speed_ms":420},"theme":{"bg":"#2A103E","accent":"#F9D860","text":"#FFF9E6"},"timeline":{"loop":true,"duration_ms":1200,"keyframes":[{"at_ms":0,"effect":"celebrate","speed_ms":420,"theme":{"bg":"#2A103E","accent":"#F9D860","text":"#FFF9E6"}},{"at_ms":1200,"effect":"pulse","speed_ms":280,"theme":{"bg":"#3E1A52","accent":"#FFD97D","text":"#FFFDF2"}}]},"transition":{"effect":"zoom","duration_ms":300}})JSON"},
    {"/story/screens/SCENE_SEARCH.json",
     R"JSON({"id":"SCENE_SEARCH","title":"RECHERCHE","subtitle":"Analyse des indices","symbol":"SCAN","effect":"scan","visual":{"show_title":false,"show_symbol":true,"effect_speed_ms":920},"theme":{"bg":"#05261F","accent":"#35E7B0","text":"#EFFFF8"},"timeline":{"loop":true,"duration_ms":3000,"keyframes":[{"at_ms":0,"effect":"scan","speed_ms":920,"theme":{"bg":"#05261F","accent":"#35E7B0","text":"#EFFFF8"}},{"at_ms":1600,"effect":"pulse","speed_ms":600,"theme":{"bg":"#07322A","accent":"#67F0C4","text":"#F2FFF9"}},{"at_ms":3000,"effect":"scan","speed_ms":820,"theme":{"bg":"#05261F","accent":"#35E7B0","text":"#EFFFF8"}}]},"transition":{"effect":"slide_left","duration_ms":230}})JSON"},
    {"/story/screens/SCENE_WIN.json",
     R"JSON({"id":"SCENE_WIN","title":"VICTOIRE","subtitle":"Etape validee","symbol":"WIN","effect":"celebrate","visual":{"show_title":false,"show_symbol":true,"effect_speed_ms":420},"theme":{"bg":"#231038","accent":"#F4CB4A","text":"#FFF8E2"},"timeline":{"loop":true,"duration_ms":1000,"keyframes":[{"at_ms":0,"effect":"celebrate","speed_ms":420,"theme":{"bg":"#231038","accent":"#F4CB4A","text":"#FFF8E2"}},{"at_ms":1000,"effect":"blink","speed_ms":240,"theme":{"bg":"#341A4D","accent":"#FFE083","text":"#FFFDF3"}}]},"transition":{"effect":"zoom","duration_ms":280}})JSON"},
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
    return false;
  }
  const uint8_t card_type = SD_MMC.cardType();
  if (card_type == CARD_NONE) {
    SD_MMC.end();
    Serial.println("[FS] SD_MMC card not detected");
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

  const String id = scene_id;
  const String slug = sceneIdToSlug(scene_id);
  const String candidates[] = {
      "/story/screens/" + id + ".json",
      "/story/screens/" + slug + ".json",
      "/screens/" + id + ".json",
      "/screens/" + slug + ".json",
      "/scenarios/data/scene_" + slug + ".json",
      "/sd/story/screens/" + id + ".json",
      "/sd/story/screens/" + slug + ".json",
  };

  for (const String& candidate : candidates) {
    String payload;
    String origin;
    if (!readTextFileWithOrigin(candidate.c_str(), &payload, &origin)) {
      continue;
    }
    Serial.printf("[FS] scene %s -> %s\n", scene_id, origin.c_str());
    return payload;
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
    return false;
  }
  const String sd_path = stripSdPrefix(src_path);
  File src = SD_MMC.open(sd_path.c_str(), "r");
  if (!src) {
    return false;
  }
  if (!ensureParentDirectoriesOnLittleFs(dst_path)) {
    src.close();
    return false;
  }
  File dst = LittleFS.open(dst_path, "w");
  if (!dst) {
    src.close();
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
    if (asset.path == nullptr || asset.payload == nullptr) {
      continue;
    }
    if (pathExistsOnLittleFs(asset.path)) {
      continue;
    }
    if (writeTextToLittleFs(asset.path, asset.payload)) {
      ++written_count;
    }
  }
  if (written_count > 0U) {
    Serial.printf("[FS] provisioned embedded story assets: %u\n", written_count);
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
