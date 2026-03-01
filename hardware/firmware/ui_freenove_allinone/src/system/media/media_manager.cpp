// media_manager.cpp - media catalog + playback + simulated recorder hooks.
#include "media_manager.h"

#include <ArduinoJson.h>
#include <FS.h>
#include <LittleFS.h>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <vector>

#include "audio_manager.h"

namespace {

void copyText(char* out, size_t out_size, const char* text) {
  if (out == nullptr || out_size == 0U) {
    return;
  }
  if (text == nullptr) {
    out[0] = '\0';
    return;
  }
  std::strncpy(out, text, out_size - 1U);
  out[out_size - 1U] = '\0';
}

char toLowerAscii(char ch) {
  return static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
}

bool equalsIgnoreCase(const char* lhs, const char* rhs) {
  if (lhs == nullptr || rhs == nullptr) {
    return false;
  }
  size_t index = 0U;
  for (;; ++index) {
    const char l = lhs[index];
    const char r = rhs[index];
    if (l == '\0' && r == '\0') {
      return true;
    }
    if (toLowerAscii(l) != toLowerAscii(r)) {
      return false;
    }
  }
}

bool isAsciiDigit(char ch) {
  return ch >= '0' && ch <= '9';
}

int compareNaturalPath(const String& lhs, const String& rhs) {
  const char* a = lhs.c_str();
  const char* b = rhs.c_str();
  size_t ia = 0U;
  size_t ib = 0U;
  while (a[ia] != '\0' && b[ib] != '\0') {
    const char ca = a[ia];
    const char cb = b[ib];
    if (isAsciiDigit(ca) && isAsciiDigit(cb)) {
      unsigned long va = 0UL;
      unsigned long vb = 0UL;
      while (isAsciiDigit(a[ia])) {
        va = (va * 10UL) + static_cast<unsigned long>(a[ia] - '0');
        ++ia;
      }
      while (isAsciiDigit(b[ib])) {
        vb = (vb * 10UL) + static_cast<unsigned long>(b[ib] - '0');
        ++ib;
      }
      if (va < vb) {
        return -1;
      }
      if (va > vb) {
        return 1;
      }
      continue;
    }
    const char la = toLowerAscii(ca);
    const char lb = toLowerAscii(cb);
    if (la < lb) {
      return -1;
    }
    if (la > lb) {
      return 1;
    }
    ++ia;
    ++ib;
  }
  if (a[ia] == '\0' && b[ib] == '\0') {
    return 0;
  }
  return (a[ia] == '\0') ? -1 : 1;
}

}  // namespace

bool MediaManager::begin(const Config& config) {
  config_ = config;
  copyText(config_.music_dir, sizeof(config_.music_dir), normalizeDir(config.music_dir).c_str());
  copyText(config_.picture_dir, sizeof(config_.picture_dir), normalizeDir(config.picture_dir).c_str());
  copyText(config_.record_dir, sizeof(config_.record_dir), normalizeDir(config.record_dir).c_str());
  if (config_.record_max_seconds == 0U) {
    config_.record_max_seconds = 30U;
  }
  if (config_.record_max_seconds > 1800U) {
    config_.record_max_seconds = 1800U;
  }

  snapshot_ = Snapshot();
  snapshot_.ready = true;
  snapshot_.record_limit_seconds = config_.record_max_seconds;
  copyText(snapshot_.music_dir, sizeof(snapshot_.music_dir), config_.music_dir);
  copyText(snapshot_.picture_dir, sizeof(snapshot_.picture_dir), config_.picture_dir);
  copyText(snapshot_.record_dir, sizeof(snapshot_.record_dir), config_.record_dir);
  ensureDir(config_.music_dir);
  ensureDir(config_.picture_dir);
  ensureDir(config_.record_dir);
  return true;
}

void MediaManager::update(uint32_t now_ms, AudioManager* audio) {
  if (audio != nullptr && snapshot_.playing && !audio->isPlaying()) {
    snapshot_.playing = false;
    snapshot_.playing_path[0] = '\0';
  }
  if (snapshot_.recording) {
    const uint32_t elapsed_ms = now_ms - snapshot_.record_started_ms;
    const uint16_t elapsed_seconds = static_cast<uint16_t>(elapsed_ms / 1000U);
    snapshot_.record_elapsed_seconds = elapsed_seconds;
    if (snapshot_.record_limit_seconds > 0U && elapsed_seconds >= snapshot_.record_limit_seconds) {
      stopRecording();
    }
  }
}

void MediaManager::noteStepChange() {
  if (config_.auto_stop_record_on_step_change && snapshot_.recording) {
    stopRecording();
  }
}

bool MediaManager::listFiles(const char* kind, String* out_json) const {
  if (out_json == nullptr) {
    return false;
  }
  out_json->remove(0);
  const String dir = resolveKindDir(kind);
  if (dir.isEmpty()) {
    return false;
  }
  if (!LittleFS.exists(dir.c_str())) {
    *out_json = "[]";
    return true;
  }

  File folder = LittleFS.open(dir.c_str(), "r");
  if (!folder || !folder.isDirectory()) {
    return false;
  }

  std::vector<String> listed_paths;
  listed_paths.reserve(32U);
  File entry = folder.openNextFile();
  while (entry) {
    if (!entry.isDirectory()) {
      String path = entry.name();
      if (!path.startsWith("/")) {
        path = "/" + path;
      }
      listed_paths.push_back(path);
    }
    entry.close();
    entry = folder.openNextFile();
  }
  folder.close();

  // Keep media list stable and human-friendly for MP3/photo browsing.
  std::sort(listed_paths.begin(), listed_paths.end(), [](const String& lhs, const String& rhs) {
    return compareNaturalPath(lhs, rhs) < 0;
  });

  DynamicJsonDocument document(4096);
  JsonArray files = document.to<JsonArray>();
  for (const String& path : listed_paths) {
    files.add(path);
  }
  serializeJson(files, *out_json);
  return true;
}

bool MediaManager::play(const char* path, AudioManager* audio) {
  if (audio == nullptr || path == nullptr || path[0] == '\0') {
    setLastError("media_play_invalid_args");
    return false;
  }
  String normalized_path = path;
  normalized_path.trim();
  if (normalized_path.isEmpty()) {
    setLastError("media_play_empty_path");
    return false;
  }
  if (!normalized_path.startsWith("/")) {
    normalized_path = String(config_.music_dir) + "/" + normalized_path;
  }

  const bool ok = audio->play(normalized_path.c_str());
  snapshot_.playing = ok;
  if (ok) {
    copyText(snapshot_.playing_path, sizeof(snapshot_.playing_path), normalized_path.c_str());
    clearLastError();
  } else {
    setLastError("media_play_failed");
  }
  return ok;
}

bool MediaManager::stop(AudioManager* audio) {
  if (audio != nullptr) {
    audio->stop();
  }
  snapshot_.playing = false;
  snapshot_.playing_path[0] = '\0';
  clearLastError();
  return true;
}

bool MediaManager::startRecording(uint16_t seconds, const char* filename_hint) {
  if (seconds == 0U) {
    seconds = config_.record_max_seconds;
  }
  if (seconds > config_.record_max_seconds) {
    seconds = config_.record_max_seconds;
  }
  if (seconds == 0U) {
    seconds = 1U;
  }
  if (snapshot_.recording) {
    setLastError("recorder_already_running");
    return false;
  }
  if (!ensureDir(config_.record_dir)) {
    setLastError("recorder_dir_missing");
    return false;
  }

  const String filename = sanitizeFilename(filename_hint, "record", ".wav");
  const String path = String(config_.record_dir) + "/" + filename;
  if (!writeEmptyWav(path.c_str())) {
    setLastError("recorder_create_failed");
    return false;
  }

  snapshot_.recording = true;
  snapshot_.record_limit_seconds = seconds;
  snapshot_.record_started_ms = millis();
  snapshot_.record_elapsed_seconds = 0U;
  copyText(snapshot_.record_file, sizeof(snapshot_.record_file), path.c_str());
  clearLastError();
  return true;
}

bool MediaManager::stopRecording() {
  if (!snapshot_.recording) {
    return true;
  }
  const uint32_t elapsed_ms = millis() - snapshot_.record_started_ms;
  snapshot_.record_elapsed_seconds = static_cast<uint16_t>(elapsed_ms / 1000U);
  snapshot_.recording = false;
  clearLastError();
  return true;
}

MediaManager::Snapshot MediaManager::snapshot() const {
  return snapshot_;
}

void MediaManager::setLastError(const char* message) {
  snapshot_.last_ok = false;
  copyText(snapshot_.last_error, sizeof(snapshot_.last_error), message);
}

void MediaManager::clearLastError() {
  snapshot_.last_ok = true;
  snapshot_.last_error[0] = '\0';
}

String MediaManager::normalizeDir(const char* path) const {
  if (path == nullptr || path[0] == '\0') {
    return String("/");
  }
  String normalized = path;
  normalized.trim();
  if (normalized.isEmpty()) {
    return String("/");
  }
  if (!normalized.startsWith("/")) {
    normalized = "/" + normalized;
  }
  if (normalized.length() > 1U && normalized.endsWith("/")) {
    normalized.remove(normalized.length() - 1U);
  }
  return normalized;
}

String MediaManager::resolveKindDir(const char* kind) const {
  if (kind == nullptr) {
    return String();
  }
  if (equalsIgnoreCase(kind, "picture") || equalsIgnoreCase(kind, "pictures")) {
    return config_.picture_dir;
  }
  if (equalsIgnoreCase(kind, "music") || equalsIgnoreCase(kind, "audio")) {
    return config_.music_dir;
  }
  if (equalsIgnoreCase(kind, "recorder") || equalsIgnoreCase(kind, "record") || equalsIgnoreCase(kind, "records")) {
    return config_.record_dir;
  }
  return String();
}

String MediaManager::sanitizeFilename(const char* hint, const char* default_prefix, const char* extension) const {
  String filename = (hint != nullptr) ? hint : "";
  filename.trim();
  if (filename.isEmpty()) {
    filename = default_prefix;
    filename += "_";
    filename += String(static_cast<unsigned long>(millis()));
  }
  for (size_t index = 0U; index < filename.length(); ++index) {
    const char ch = filename[index];
    const bool keep = std::isalnum(static_cast<unsigned char>(ch)) || ch == '_' || ch == '-' || ch == '.';
    if (!keep) {
      filename.setCharAt(index, '_');
    }
  }
  if (extension != nullptr && extension[0] != '\0' && !filename.endsWith(extension)) {
    filename += extension;
  }
  return filename;
}

bool MediaManager::ensureDir(const char* path) const {
  const String normalized = normalizeDir(path);
  if (normalized.isEmpty()) {
    return false;
  }
  if (LittleFS.exists(normalized.c_str())) {
    return true;
  }
  return LittleFS.mkdir(normalized.c_str());
}

bool MediaManager::writeEmptyWav(const char* path) const {
  if (path == nullptr || path[0] == '\0') {
    return false;
  }
  File file = LittleFS.open(path, "w");
  if (!file) {
    return false;
  }

  const uint32_t sample_rate = 16000UL;
  const uint16_t channels = 1U;
  const uint16_t bits_per_sample = 16U;
  const uint32_t data_size = 0UL;
  const uint32_t byte_rate = sample_rate * channels * (bits_per_sample / 8U);
  const uint16_t block_align = channels * (bits_per_sample / 8U);
  const uint32_t chunk_size = 36UL + data_size;

  file.write(reinterpret_cast<const uint8_t*>("RIFF"), 4U);
  file.write(reinterpret_cast<const uint8_t*>(&chunk_size), sizeof(chunk_size));
  file.write(reinterpret_cast<const uint8_t*>("WAVE"), 4U);
  file.write(reinterpret_cast<const uint8_t*>("fmt "), 4U);
  const uint32_t fmt_size = 16UL;
  const uint16_t audio_format = 1U;
  file.write(reinterpret_cast<const uint8_t*>(&fmt_size), sizeof(fmt_size));
  file.write(reinterpret_cast<const uint8_t*>(&audio_format), sizeof(audio_format));
  file.write(reinterpret_cast<const uint8_t*>(&channels), sizeof(channels));
  file.write(reinterpret_cast<const uint8_t*>(&sample_rate), sizeof(sample_rate));
  file.write(reinterpret_cast<const uint8_t*>(&byte_rate), sizeof(byte_rate));
  file.write(reinterpret_cast<const uint8_t*>(&block_align), sizeof(block_align));
  file.write(reinterpret_cast<const uint8_t*>(&bits_per_sample), sizeof(bits_per_sample));
  file.write(reinterpret_cast<const uint8_t*>("data"), 4U);
  file.write(reinterpret_cast<const uint8_t*>(&data_size), sizeof(data_size));
  file.close();
  return true;
}
