// audio_manager.cpp - audio playback over I2S with ESP32-audioI2S backend.
#include "audio_manager.h"

#include <Audio.h>
#include <FS.h>
#include <LittleFS.h>
#include <cctype>
#include <cstring>

#if defined(ARDUINO_ARCH_ESP32) && __has_include(<SD_MMC.h>)
#include <SD_MMC.h>
#define ZACUS_HAS_SD_AUDIO 1
#else
#define ZACUS_HAS_SD_AUDIO 0
#endif

#if defined(ARDUINO_ARCH_ESP32)
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#endif

namespace {

constexpr char kDiagnosticTrackPath[] = "/music/boot_radio.mp3";
constexpr uint8_t kMaxTrackPathLen = 120U;
constexpr uint16_t kBitrateScanBytes = 4096U;
constexpr uint8_t kAudioDoneQueueDepth = 6U;
constexpr uint16_t kAudioDoneTrackLen = 96U;
constexpr uint16_t kAudioPumpTaskStackWords = 4096U;
constexpr uint8_t kAudioPumpTaskPriority = 3U;
constexpr uint8_t kAudioPumpTaskCore = 1U;
constexpr uint16_t kAudioPumpActiveDelayMs = 1U;
constexpr uint16_t kAudioPumpIdleDelayMs = 4U;
constexpr uint16_t kAudioStateLockTimeoutMs = 20U;

struct AudioPinProfile {
  int bck;
  int ws;
  int dout;
  const char* label;
};

constexpr AudioPinProfile kAudioPinProfiles[] = {
    {FREENOVE_I2S_BCK, FREENOVE_I2S_WS, FREENOVE_I2S_DOUT, "sketch19"},
    {FREENOVE_I2S_WS, FREENOVE_I2S_BCK, FREENOVE_I2S_DOUT, "swap_bck_ws"},
    {FREENOVE_I2S_BCK, FREENOVE_I2S_WS, 2, "dout2_alt"},
};

constexpr uint8_t kAudioPinProfileCount =
    static_cast<uint8_t>(sizeof(kAudioPinProfiles) / sizeof(kAudioPinProfiles[0]));

struct AudioFxProfile {
  const char* label;
  int8_t low;
  int8_t mid;
  int8_t high;
};

constexpr AudioFxProfile kAudioFxProfiles[] = {
    {"flat", 0, 0, 0},
    {"soft", -4, 1, -2},
    {"warm", 3, 0, -2},
    {"bright", -2, 1, 4},
};

constexpr uint8_t kAudioFxProfileCount =
    static_cast<uint8_t>(sizeof(kAudioFxProfiles) / sizeof(kAudioFxProfiles[0]));

constexpr uint16_t kAudioMp3Mpeg1LayerIII[16] = {0U, 32U, 40U, 48U, 56U, 64U, 80U, 96U, 112U, 128U, 160U, 192U, 224U, 256U, 320U, 0U};
constexpr uint16_t kAudioMp3Mpeg2LayerIII[16] = {0U, 8U, 16U, 24U, 32U, 40U, 48U, 56U, 64U, 80U, 96U, 112U, 128U, 160U, 192U, 0U};

char toLowerAscii(char value) {
  return static_cast<char>(std::tolower(static_cast<unsigned char>(value)));
}

bool endsWithIgnoreCase(const String& value, const char* suffix) {
  if (suffix == nullptr) {
    return false;
  }
  const size_t suffix_len = std::strlen(suffix);
  if (suffix_len == 0U || value.length() < suffix_len) {
    return false;
  }
  const size_t offset = value.length() - suffix_len;
  for (size_t index = 0U; index < suffix_len; ++index) {
    if (toLowerAscii(value.charAt(offset + index)) != toLowerAscii(suffix[index])) {
      return false;
    }
  }
  return true;
}

bool resolveFileSystem(bool use_sd, fs::FS*& out_fs, const char*& out_label) {
  out_fs = &LittleFS;
  out_label = "littlefs";
  if (!use_sd) {
    return true;
  }
#if ZACUS_HAS_SD_AUDIO
  out_fs = &SD_MMC;
  out_label = "sd";
  return true;
#else
  return false;
#endif
}

uint16_t parseMp3BitrateHeader(const uint8_t* header, size_t header_len) {
  if (header == nullptr || header_len < 4U) {
    return 0U;
  }
  if (header[0] != 0xFFU || (header[1] & 0xE0U) != 0xE0U) {
    return 0U;
  }
  const uint8_t version = (header[1] >> 3U) & 0x03U;
  const uint8_t layer = (header[1] >> 1U) & 0x03U;
  const uint8_t bitrate_index = (header[2] >> 4U) & 0x0FU;
  if (layer != 0x01U || bitrate_index == 0x00U || bitrate_index == 0x0FU) {
    return 0U;
  }
  if (version == 0x03U) {
    return kAudioMp3Mpeg1LayerIII[bitrate_index];
  }
  return kAudioMp3Mpeg2LayerIII[bitrate_index];
}

void skipId3V2Header(File& file) {
  if (!file || file.size() < 10U) {
    return;
  }
  const uint32_t cursor = file.position();
  uint8_t id3[10] = {0};
  if (file.read(id3, sizeof(id3)) != sizeof(id3)) {
    file.seek(cursor);
    return;
  }
  if (id3[0] != 'I' || id3[1] != 'D' || id3[2] != '3') {
    file.seek(cursor);
    return;
  }
  const uint32_t size = ((static_cast<uint32_t>(id3[6] & 0x7FU) << 21U) |
                         (static_cast<uint32_t>(id3[7] & 0x7FU) << 14U) |
                         (static_cast<uint32_t>(id3[8] & 0x7FU) << 7U) |
                         (static_cast<uint32_t>(id3[9] & 0x7FU)));
  file.seek(10U + size);
}

uint16_t detectMp3Bitrate(fs::FS& file_system, const char* path) {
  if (path == nullptr || path[0] == '\0') {
    return 0U;
  }
  File file = file_system.open(path, "r");
  if (!file || file.isDirectory()) {
    return 0U;
  }
  skipId3V2Header(file);

  uint8_t window[4] = {0};
  size_t filled = 0U;
  uint16_t scanned = 0U;
  while (file.available() && scanned < kBitrateScanBytes) {
    const int raw = file.read();
    if (raw < 0) {
      break;
    }
    if (filled < 4U) {
      window[filled++] = static_cast<uint8_t>(raw);
      if (filled < 4U) {
        continue;
      }
    } else {
      window[0] = window[1];
      window[1] = window[2];
      window[2] = window[3];
      window[3] = static_cast<uint8_t>(raw);
    }
    const uint16_t bitrate = parseMp3BitrateHeader(window, sizeof(window));
    if (bitrate > 0U) {
      file.close();
      return bitrate;
    }
    ++scanned;
  }
  file.close();
  return 0U;
}

struct AudioDoneEvent {
  char track[kAudioDoneTrackLen];
};

}  // namespace

struct AudioManager::AudioRtosState {
#if defined(ARDUINO_ARCH_ESP32)
  TaskHandle_t pump_task = nullptr;
  SemaphoreHandle_t state_mutex = nullptr;
  QueueHandle_t done_queue = nullptr;
#endif
  bool running = false;
};

AudioManager::AudioManager() {
  createRtosState();
}

AudioManager::~AudioManager() {
  stopAudioPump();
  if (takeStateLock(kAudioStateLockTimeoutMs)) {
    if (player_ != nullptr) {
      player_->stopSong();
    }
    clearTrackState();
    releaseStateLock();
  }
  delete player_;
  player_ = nullptr;
  destroyRtosState();
}

void AudioManager::createRtosState() {
  if (rtos_state_ != nullptr) {
    return;
  }
  rtos_state_ = new AudioRtosState();
  if (rtos_state_ == nullptr) {
    return;
  }
#if defined(ARDUINO_ARCH_ESP32)
  rtos_state_->state_mutex = xSemaphoreCreateMutex();
  rtos_state_->done_queue = xQueueCreate(kAudioDoneQueueDepth, sizeof(AudioDoneEvent));
  if (rtos_state_->state_mutex == nullptr || rtos_state_->done_queue == nullptr) {
    if (rtos_state_->done_queue != nullptr) {
      vQueueDelete(rtos_state_->done_queue);
      rtos_state_->done_queue = nullptr;
    }
    if (rtos_state_->state_mutex != nullptr) {
      vSemaphoreDelete(rtos_state_->state_mutex);
      rtos_state_->state_mutex = nullptr;
    }
    delete rtos_state_;
    rtos_state_ = nullptr;
    Serial.println("[AUDIO] RTOS state alloc failed");
  }
#endif
}

void AudioManager::destroyRtosState() {
  if (rtos_state_ == nullptr) {
    return;
  }
#if defined(ARDUINO_ARCH_ESP32)
  if (rtos_state_->done_queue != nullptr) {
    vQueueDelete(rtos_state_->done_queue);
    rtos_state_->done_queue = nullptr;
  }
  if (rtos_state_->state_mutex != nullptr) {
    vSemaphoreDelete(rtos_state_->state_mutex);
    rtos_state_->state_mutex = nullptr;
  }
#endif
  delete rtos_state_;
  rtos_state_ = nullptr;
}

bool AudioManager::takeStateLock(uint32_t timeout_ms) const {
#if defined(ARDUINO_ARCH_ESP32)
  if (rtos_state_ == nullptr || rtos_state_->state_mutex == nullptr) {
    return true;
  }
  const TickType_t ticks = (timeout_ms == 0U) ? 0U : pdMS_TO_TICKS(timeout_ms);
  return xSemaphoreTake(rtos_state_->state_mutex, ticks) == pdTRUE;
#else
  (void)timeout_ms;
  return true;
#endif
}

void AudioManager::releaseStateLock() const {
#if defined(ARDUINO_ARCH_ESP32)
  if (rtos_state_ == nullptr || rtos_state_->state_mutex == nullptr) {
    return;
  }
  xSemaphoreGive(rtos_state_->state_mutex);
#endif
}

bool AudioManager::startAudioPump() {
#if defined(ARDUINO_ARCH_ESP32)
  if (rtos_state_ == nullptr || rtos_state_->state_mutex == nullptr || rtos_state_->done_queue == nullptr) {
    return false;
  }
  if (rtos_state_->running && rtos_state_->pump_task != nullptr) {
    return true;
  }
  rtos_state_->running = true;
  const BaseType_t created = xTaskCreatePinnedToCore(AudioManager::audioPumpTaskEntry,
                                                      "audio_pump",
                                                      kAudioPumpTaskStackWords,
                                                      this,
                                                      kAudioPumpTaskPriority,
                                                      &rtos_state_->pump_task,
                                                      kAudioPumpTaskCore);
  if (created != pdPASS) {
    rtos_state_->running = false;
    rtos_state_->pump_task = nullptr;
    Serial.println("[AUDIO] failed to start pump task");
    return false;
  }
  return true;
#else
  return false;
#endif
}

void AudioManager::stopAudioPump() {
#if defined(ARDUINO_ARCH_ESP32)
  if (rtos_state_ == nullptr || !rtos_state_->running) {
    pump_task_enabled_ = false;
    return;
  }
  rtos_state_->running = false;
  const uint32_t deadline_ms = millis() + 800U;
  while (rtos_state_->pump_task != nullptr && static_cast<int32_t>(deadline_ms - millis()) > 0) {
    delay(1);
  }
  if (rtos_state_->pump_task != nullptr) {
    vTaskDelete(rtos_state_->pump_task);
    rtos_state_->pump_task = nullptr;
  }
#endif
  pump_task_enabled_ = false;
}

void AudioManager::audioPumpTaskEntry(void* arg) {
  reinterpret_cast<AudioManager*>(arg)->audioPumpLoop();
}

void AudioManager::audioPumpLoop() {
#if defined(ARDUINO_ARCH_ESP32)
  while (rtos_state_ != nullptr && rtos_state_->running) {
    bool active = false;
    bool finished = false;
    char finished_track[kAudioDoneTrackLen] = {0};
    if (takeStateLock(0U)) {
      if (player_ != nullptr && playing_) {
        active = true;
        player_->loop();
        if (!player_->isRunning()) {
          std::strncpy(finished_track, current_track_.c_str(), sizeof(finished_track) - 1U);
          finished_track[sizeof(finished_track) - 1U] = '\0';
          clearTrackState();
          finished = true;
        }
      }
      releaseStateLock();
    }
    if (finished) {
      enqueuePlaybackDone(finished_track);
    }
    vTaskDelay(pdMS_TO_TICKS(active ? kAudioPumpActiveDelayMs : kAudioPumpIdleDelayMs));
  }
  if (rtos_state_ != nullptr) {
    rtos_state_->running = false;
    rtos_state_->pump_task = nullptr;
  }
  vTaskDelete(nullptr);
#endif
}

bool AudioManager::ensurePlayer() {
  if (player_ != nullptr) {
    return true;
  }
  player_ = new Audio();
  if (player_ == nullptr) {
    Serial.println("[AUDIO] alloc failed for ESP32-audioI2S player");
    return false;
  }
  applyOutputProfile();
  player_->setVolume(volume_);
  applyFxProfile();
  return true;
}

bool AudioManager::begin() {
  if (!takeStateLock(kAudioStateLockTimeoutMs)) {
    Serial.println("[AUDIO] begin lock timeout");
    return false;
  }
  const bool ready = ensurePlayer();
  releaseStateLock();
  if (!ready) {
    return false;
  }
  begun_ = true;
  pump_task_enabled_ = startAudioPump();
  Serial.printf("[AUDIO] backend=ESP32-audioI2S profile=%u:%s fx=%u:%s vol=%u\n",
                output_profile_,
                outputProfileLabel(output_profile_),
                fx_profile_,
                fxProfileLabel(fx_profile_),
                volume_);
  Serial.printf("[AUDIO] pump task=%u\n", pump_task_enabled_ ? 1U : 0U);
  return true;
}

bool AudioManager::normalizeTrackPath(const char* input, String& out_path, bool& out_use_sd) const {
  if (input == nullptr || input[0] == '\0') {
    return false;
  }
  String path = input;
  path.trim();
  if (path.isEmpty()) {
    return false;
  }

  out_use_sd = false;
  if (path.startsWith("/littlefs/")) {
    path.remove(0, 9);
  } else if (path.startsWith("/sd/")) {
    out_use_sd = true;
    path.remove(0, 3);
  } else if (path.startsWith("sd:/")) {
    out_use_sd = true;
    path.remove(0, 3);
  } else if (path.startsWith("littlefs:/")) {
    path.remove(0, 9);
  }

  if (!path.startsWith("/")) {
    path = "/" + path;
  }
  if (path.length() > kMaxTrackPathLen) {
    Serial.printf("[AUDIO] normalized path too long: %s\n", input);
    return false;
  }
  out_path = path;
  return true;
}

bool AudioManager::trackExists(const String& path, bool use_sd) const {
  fs::FS* file_system = nullptr;
  const char* fs_label = nullptr;
  if (!resolveFileSystem(use_sd, file_system, fs_label) || file_system == nullptr) {
    Serial.printf("[AUDIO] fs unavailable for path=%s use_sd=%u\n",
                  path.c_str(),
                  use_sd ? 1U : 0U);
    return false;
  }
  const bool exists = file_system->exists(path.c_str());
  if (!exists) {
    Serial.printf("[AUDIO] file missing fs=%s path=%s\n", fs_label, path.c_str());
  }
  return exists;
}

bool AudioManager::detectTrackCodecAndBitrate(const String& path,
                                              bool use_sd,
                                              AudioCodec& codec,
                                              uint16_t& bitrate_kbps) const {
  codec = AudioCodec::kUnknown;
  bitrate_kbps = 0U;
  if (path.isEmpty()) {
    return false;
  }

  if (endsWithIgnoreCase(path, ".mp3")) {
    codec = AudioCodec::kMp3;
    fs::FS* file_system = nullptr;
    const char* fs_label = nullptr;
    if (resolveFileSystem(use_sd, file_system, fs_label) && file_system != nullptr) {
      bitrate_kbps = detectMp3Bitrate(*file_system, path.c_str());
    }
    return true;
  }
  if (endsWithIgnoreCase(path, ".wav")) {
    codec = AudioCodec::kWav;
    return true;
  }
  if (endsWithIgnoreCase(path, ".aac") || endsWithIgnoreCase(path, ".m4a")) {
    codec = AudioCodec::kAac;
    return true;
  }
  if (endsWithIgnoreCase(path, ".flac")) {
    codec = AudioCodec::kFlac;
    return true;
  }
  return true;
}

const char* AudioManager::codecLabel(AudioCodec codec) const {
  switch (codec) {
    case AudioCodec::kMp3:
      return "mp3";
    case AudioCodec::kWav:
      return "wav";
    case AudioCodec::kAac:
      return "aac";
    case AudioCodec::kFlac:
      return "flac";
    case AudioCodec::kUnknown:
    default:
      return "unknown";
  }
}

bool AudioManager::openTrack(const String& path, bool use_sd) {
  if (!ensurePlayer()) {
    return false;
  }

  fs::FS* file_system = nullptr;
  const char* fs_label = nullptr;
  if (!resolveFileSystem(use_sd, file_system, fs_label) || file_system == nullptr) {
    return false;
  }

  if (!player_->connecttoFS(*file_system, path.c_str())) {
    Serial.printf("[AUDIO] connecttoFS failed fs=%s path=%s\n", fs_label, path.c_str());
    return false;
  }
  return true;
}

bool AudioManager::beginTrackPlayback(const String& path,
                                      bool use_sd,
                                      AudioCodec codec,
                                      uint16_t bitrate_kbps,
                                      bool diagnostic_tone) {
  if (!openTrack(path, use_sd)) {
    return false;
  }

  current_track_ = use_sd ? String("/sd") + path : path;
  active_codec_ = codec;
  active_bitrate_kbps_ = bitrate_kbps;
  active_use_sd_ = use_sd;
  using_diagnostic_tone_ = diagnostic_tone;
  playing_ = true;
  reopen_earliest_ms_ = 0U;

  Serial.printf("[AUDIO] play start track=%s codec=%s bitrate=%u profile=%u:%s fx=%u:%s vol=%u\n",
                current_track_.c_str(),
                codecLabel(active_codec_),
                active_bitrate_kbps_,
                output_profile_,
                outputProfileLabel(output_profile_),
                fx_profile_,
                fxProfileLabel(fx_profile_),
                volume_);
  if (diagnostic_tone) {
    Serial.printf("[AUDIO] diagnostic playback path=%s\n", current_track_.c_str());
  }
  return true;
}

void AudioManager::scheduleTrackStart(const String& path,
                                      bool use_sd,
                                      AudioCodec codec,
                                      uint16_t bitrate_kbps,
                                      bool diagnostic_tone,
                                      uint32_t earliest_ms) {
  pending_track_ = path;
  pending_use_sd_ = use_sd;
  pending_codec_ = codec;
  pending_bitrate_kbps_ = bitrate_kbps;
  pending_diagnostic_tone_ = diagnostic_tone;
  pending_start_ = true;
  reopen_earliest_ms_ = earliest_ms;
}

void AudioManager::tryStartPendingTrack(uint32_t now_ms) {
  if (!pending_start_) {
    return;
  }
  if (now_ms < reopen_earliest_ms_) {
    return;
  }
  if (!ensurePlayer()) {
    pending_start_ = false;
    return;
  }
  if (player_->isRunning()) {
    return;
  }

  String pending_track = pending_track_;
  const bool pending_use_sd = pending_use_sd_;
  const AudioCodec pending_codec = pending_codec_;
  const uint16_t pending_bitrate_kbps = pending_bitrate_kbps_;
  const bool pending_diagnostic_tone = pending_diagnostic_tone_;
  pending_start_ = false;
  pending_track_.remove(0);
  pending_diagnostic_tone_ = false;

  if (!beginTrackPlayback(pending_track,
                          pending_use_sd,
                          pending_codec,
                          pending_bitrate_kbps,
                          pending_diagnostic_tone)) {
    Serial.printf("[AUDIO] deferred start failed path=%s\n", pending_track.c_str());
  }
}

bool AudioManager::requestPlay(const char* filename, bool diagnostic_tone) {
  String normalized_path;
  bool use_sd = false;
  if (!normalizeTrackPath(filename, normalized_path, use_sd)) {
    Serial.printf("[AUDIO] invalid path: %s\n", filename != nullptr ? filename : "<null>");
    return false;
  }
  if (!trackExists(normalized_path, use_sd)) {
    return false;
  }

  AudioCodec codec = AudioCodec::kUnknown;
  uint16_t bitrate_kbps = 0U;
  detectTrackCodecAndBitrate(normalized_path, use_sd, codec, bitrate_kbps);

  if (!takeStateLock(kAudioStateLockTimeoutMs)) {
    Serial.println("[AUDIO] requestPlay lock timeout");
    return false;
  }
  if (!ensurePlayer()) {
    releaseStateLock();
    return false;
  }
  if (player_->isRunning() || playing_) {
    pending_start_ = false;
    pending_track_.remove(0);
    pending_diagnostic_tone_ = false;
    player_->stopSong();
    clearTrackState();
    reopen_earliest_ms_ = millis() + 80U;
  }
  const uint32_t now_ms = millis();
  if (now_ms < reopen_earliest_ms_) {
    scheduleTrackStart(normalized_path,
                       use_sd,
                       codec,
                       bitrate_kbps,
                       diagnostic_tone,
                       reopen_earliest_ms_);
    Serial.printf("[AUDIO] queued start track=%s wait_ms=%lu\n",
                  normalized_path.c_str(),
                  static_cast<unsigned long>(reopen_earliest_ms_ - now_ms));
    releaseStateLock();
    return true;
  }

  const bool started = beginTrackPlayback(normalized_path, use_sd, codec, bitrate_kbps, diagnostic_tone);
  releaseStateLock();
  return started;
}

bool AudioManager::play(const char* filename) {
  return requestPlay(filename, false);
}

bool AudioManager::playDiagnosticTone() {
  const bool ok = requestPlay(kDiagnosticTrackPath, true);
  if (!ok) {
    Serial.println("[AUDIO] diagnostic playback unavailable");
  }
  return ok;
}

void AudioManager::clearTrackState() {
  playing_ = false;
  using_diagnostic_tone_ = false;
  current_track_.remove(0);
  active_codec_ = AudioCodec::kUnknown;
  active_bitrate_kbps_ = 0U;
  active_use_sd_ = false;
}

void AudioManager::stop() {
  if (!takeStateLock(kAudioStateLockTimeoutMs)) {
    Serial.println("[AUDIO] stop lock timeout");
    return;
  }
  pending_start_ = false;
  pending_track_.remove(0);
  pending_diagnostic_tone_ = false;
  if (player_ != nullptr) {
    player_->stopSong();
  }
  clearTrackState();
  reopen_earliest_ms_ = millis() + 80U;
#if defined(ARDUINO_ARCH_ESP32)
  if (rtos_state_ != nullptr && rtos_state_->done_queue != nullptr) {
    xQueueReset(rtos_state_->done_queue);
  }
#endif
  releaseStateLock();
}

void AudioManager::finishPlaybackAndNotify() {
  char finished_track[kAudioDoneTrackLen] = {0};
  if (!takeStateLock(kAudioStateLockTimeoutMs)) {
    return;
  }
  std::strncpy(finished_track, current_track_.c_str(), sizeof(finished_track) - 1U);
  finished_track[sizeof(finished_track) - 1U] = '\0';
  clearTrackState();
  releaseStateLock();
  enqueuePlaybackDone(finished_track);
}

void AudioManager::enqueuePlaybackDone(const char* track) {
  const char* safe_track = (track != nullptr) ? track : "";
#if defined(ARDUINO_ARCH_ESP32)
  if (rtos_state_ != nullptr && rtos_state_->done_queue != nullptr) {
    AudioDoneEvent event = {};
    std::strncpy(event.track, safe_track, sizeof(event.track) - 1U);
    event.track[sizeof(event.track) - 1U] = '\0';
    if (xQueueSend(rtos_state_->done_queue, &event, 0U) == pdTRUE) {
      return;
    }
  }
#endif
  Serial.printf("[AUDIO] playback done track=%s\n", safe_track[0] != '\0' ? safe_track : "-");
  if (done_cb_ != nullptr) {
    done_cb_(safe_track[0] != '\0' ? safe_track : "-", done_ctx_);
  }
}

void AudioManager::processPendingPlaybackEvents() {
#if defined(ARDUINO_ARCH_ESP32)
  if (rtos_state_ == nullptr || rtos_state_->done_queue == nullptr) {
    return;
  }
  AudioDoneEvent event = {};
  while (xQueueReceive(rtos_state_->done_queue, &event, 0U) == pdTRUE) {
    const char* track = (event.track[0] != '\0') ? event.track : "-";
    Serial.printf("[AUDIO] playback done track=%s\n", track);
    if (done_cb_ != nullptr) {
      done_cb_(track, done_ctx_);
    }
  }
#endif
}

void AudioManager::update() {
  if (!begun_) {
    return;
  }
  bool finished_without_pump = false;
  char finished_track[kAudioDoneTrackLen] = {0};
  if (takeStateLock(kAudioStateLockTimeoutMs)) {
    if (player_ != nullptr) {
      tryStartPendingTrack(millis());
      if (!pump_task_enabled_ && playing_) {
        player_->loop();
        if (!player_->isRunning()) {
          std::strncpy(finished_track, current_track_.c_str(), sizeof(finished_track) - 1U);
          finished_track[sizeof(finished_track) - 1U] = '\0';
          clearTrackState();
          finished_without_pump = true;
        }
      }
    }
    releaseStateLock();
  }
  if (finished_without_pump) {
    enqueuePlaybackDone(finished_track);
  }
  processPendingPlaybackEvents();
}

bool AudioManager::isPlaying() const {
  if (!takeStateLock(kAudioStateLockTimeoutMs)) {
    return playing_;
  }
  const bool running = (playing_ && player_ != nullptr && player_->isRunning());
  releaseStateLock();
  return running;
}

void AudioManager::setVolume(uint8_t volume) {
  if (volume > FREENOVE_AUDIO_MAX_VOLUME) {
    volume = FREENOVE_AUDIO_MAX_VOLUME;
  }
  if (!takeStateLock(kAudioStateLockTimeoutMs)) {
    return;
  }
  volume_ = volume;
  if (player_ != nullptr) {
    player_->setVolume(volume_);
  }
  releaseStateLock();
}

uint8_t AudioManager::volume() const {
  if (!takeStateLock(kAudioStateLockTimeoutMs)) {
    return volume_;
  }
  const uint8_t current = volume_;
  releaseStateLock();
  return current;
}

const char* AudioManager::currentTrack() const {
  if (!takeStateLock(kAudioStateLockTimeoutMs)) {
    return "-";
  }
  if (current_track_.isEmpty()) {
    releaseStateLock();
    return "-";
  }
  std::strncpy(current_track_snapshot_, current_track_.c_str(), sizeof(current_track_snapshot_) - 1U);
  current_track_snapshot_[sizeof(current_track_snapshot_) - 1U] = '\0';
  releaseStateLock();
  return current_track_snapshot_;
}

bool AudioManager::setOutputProfile(uint8_t profile_index) {
  if (profile_index >= kAudioPinProfileCount) {
    return false;
  }
  if (!takeStateLock(kAudioStateLockTimeoutMs)) {
    return false;
  }
  output_profile_ = profile_index;
  applyOutputProfile();
  Serial.printf("[AUDIO] output profile=%u:%s\n",
                output_profile_,
                outputProfileLabel(output_profile_));
  releaseStateLock();
  return true;
}

uint8_t AudioManager::outputProfile() const {
  if (!takeStateLock(kAudioStateLockTimeoutMs)) {
    return output_profile_;
  }
  const uint8_t profile = output_profile_;
  releaseStateLock();
  return profile;
}

uint8_t AudioManager::outputProfileCount() const {
  return kAudioPinProfileCount;
}

const char* AudioManager::outputProfileLabel(uint8_t profile_index) const {
  if (profile_index >= kAudioPinProfileCount) {
    return "invalid";
  }
  return kAudioPinProfiles[profile_index].label;
}

bool AudioManager::setFxProfile(uint8_t fx_profile_index) {
  if (fx_profile_index >= kAudioFxProfileCount) {
    return false;
  }
  if (!takeStateLock(kAudioStateLockTimeoutMs)) {
    return false;
  }
  fx_profile_ = fx_profile_index;
  applyFxProfile();
  Serial.printf("[AUDIO] fx profile=%u:%s\n",
                fx_profile_,
                fxProfileLabel(fx_profile_));
  releaseStateLock();
  return true;
}

uint8_t AudioManager::fxProfile() const {
  if (!takeStateLock(kAudioStateLockTimeoutMs)) {
    return fx_profile_;
  }
  const uint8_t profile = fx_profile_;
  releaseStateLock();
  return profile;
}

uint8_t AudioManager::fxProfileCount() const {
  return kAudioFxProfileCount;
}

const char* AudioManager::fxProfileLabel(uint8_t fx_profile_index) const {
  if (fx_profile_index >= kAudioFxProfileCount) {
    return "invalid";
  }
  return kAudioFxProfiles[fx_profile_index].label;
}

const char* AudioManager::activeCodec() const {
  AudioCodec codec = AudioCodec::kUnknown;
  if (takeStateLock(kAudioStateLockTimeoutMs)) {
    codec = active_codec_;
    releaseStateLock();
  }
  return codecLabel(codec);
}

uint16_t AudioManager::activeBitrateKbps() const {
  if (!takeStateLock(kAudioStateLockTimeoutMs)) {
    return active_bitrate_kbps_;
  }
  const uint16_t bitrate = active_bitrate_kbps_;
  releaseStateLock();
  return bitrate;
}

void AudioManager::setAudioDoneCallback(AudioDoneCallback cb, void* ctx) {
  if (!takeStateLock(kAudioStateLockTimeoutMs)) {
    return;
  }
  done_cb_ = cb;
  done_ctx_ = ctx;
  releaseStateLock();
}

void AudioManager::applyOutputProfile() {
  if (player_ == nullptr) {
    return;
  }
  const AudioPinProfile& profile = kAudioPinProfiles[output_profile_];
  player_->setPinout(profile.bck, profile.ws, profile.dout);
}

void AudioManager::applyFxProfile() {
  if (player_ == nullptr) {
    return;
  }
  const AudioFxProfile& profile = kAudioFxProfiles[fx_profile_];
  player_->setTone(profile.low, profile.mid, profile.high);
}
