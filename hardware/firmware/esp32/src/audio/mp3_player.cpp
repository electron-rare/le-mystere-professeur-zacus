#include "mp3_player.h"

#include <new>
#include <strings.h>

#include <AudioFileSourceFS.h>
#include <AudioGenerator.h>
#include <AudioGeneratorAAC.h>
#include <AudioGeneratorFLAC.h>
#include <AudioGeneratorMP3.h>
#include <AudioGeneratorOpus.h>
#include <AudioGeneratorWAV.h>
#include <FS.h>
#include <SD_MMC.h>

#include "../config.h"
#include "effects/audio_effect_id.h"

namespace {

constexpr const char* kIndexPath = "/.uson_index_v1.csv";
constexpr const char* kStatePath = "/.uson_player_state_v1.json";
constexpr uint32_t kScanTickBudgetMs = 4U;
constexpr uint16_t kScanTickEntryBudget = 24U;
constexpr uint8_t kScanMaxDepth = TrackCatalog::kDefaultMaxDepth;

void copyCStr(char* out, size_t outLen, const char* value) {
  if (out == nullptr || outLen == 0U) {
    return;
  }
  out[0] = '\0';
  if (value == nullptr || value[0] == '\0') {
    return;
  }
  snprintf(out, outLen, "%s", value);
}

void setScanReason(Mp3ScanProgress* progress, const char* reason) {
  if (progress == nullptr) {
    return;
  }
  copyCStr(progress->reason, sizeof(progress->reason), reason);
}

<<<<<<< HEAD
void setFallbackReason(Mp3BackendRuntimeStats* stats, const char* reason) {
  if (stats == nullptr) {
    return;
  }
  copyCStr(stats->lastFallbackReason, sizeof(stats->lastFallbackReason), reason);
=======
void setBackendReason(Mp3BackendRuntimeStats* stats, const char* reason) {
  if (stats == nullptr) {
    return;
  }
  copyCStr(stats->lastFailureReason, sizeof(stats->lastFailureReason), reason);
}

void setFallbackPath(Mp3BackendRuntimeStats* stats, const char* path) {
  if (stats == nullptr) {
    return;
  }
  copyCStr(stats->lastFallbackPath, sizeof(stats->lastFallbackPath), path);
>>>>>>> feature/MPRC-RC1-mp3-audio
}

bool isJsonSafeChar(char c) {
  return c != '\\' && c != '"' && c != '\n' && c != '\r';
}

String jsonEscape(const String& in) {
  String out;
  out.reserve(in.length() + 8U);
  for (size_t i = 0; i < in.length(); ++i) {
    const char c = in[i];
    if (isJsonSafeChar(c)) {
      out += c;
      continue;
    }
    if (c == '"' || c == '\\') {
      out += '\\';
      out += c;
      continue;
    }
    if (c == '\n' || c == '\r') {
      out += ' ';
      continue;
    }
  }
  return out;
}

const char* scanStateToLabel(CatalogScanService::State state) {
  switch (state) {
    case CatalogScanService::State::kIdle:
      return "IDLE";
    case CatalogScanService::State::kRequested:
      return "REQUESTED";
    case CatalogScanService::State::kRunning:
      return "RUNNING";
    case CatalogScanService::State::kDone:
      return "DONE";
    case CatalogScanService::State::kFailed:
      return "FAILED";
    case CatalogScanService::State::kCanceled:
      return "CANCELED";
    default:
      return "UNKNOWN";
  }
}

}  // namespace

Mp3Player::Mp3Player(uint8_t i2sBclk,
                     uint8_t i2sLrc,
                     uint8_t i2sDout,
                     const char* mp3Path,
                     int8_t paEnablePin)
    : i2sBclk_(i2sBclk),
      i2sLrc_(i2sLrc),
      i2sDout_(i2sDout),
      paEnablePin_(paEnablePin),
      mp3Path_(mp3Path),
      audioTools_(i2sBclk, i2sLrc, i2sDout, config::kI2sOutputPort) {}

Mp3Player::~Mp3Player() {
  stop();
}

void Mp3Player::begin() {
  if (paEnablePin_ >= 0) {
    pinMode(paEnablePin_, OUTPUT);
    digitalWrite(paEnablePin_, HIGH);
  }
  scanService_.reset();
  clearScanContext();
  scanProgress_ = Mp3ScanProgress();
  scanProgress_.tickBudgetMs = static_cast<uint16_t>(kScanTickBudgetMs);
  scanProgress_.tickEntryBudget = kScanTickEntryBudget;
  setScanReason(&scanProgress_, "IDLE");
  backendStats_ = Mp3BackendRuntimeStats();
<<<<<<< HEAD
  setFallbackReason(&backendStats_, "NONE");
=======
  setBackendReason(&backendStats_, "OK");
  setFallbackPath(&backendStats_, "NONE");
>>>>>>> feature/MPRC-RC1-mp3-audio
}

void Mp3Player::update(uint32_t nowMs, bool allowPlayback) {
  refreshStorage(nowMs);
  updateDeferredStateSave(nowMs);

  if (!sdReady_ || trackCount_ == 0U) {
    stop();
    return;
  }

  if (!allowPlayback) {
    stop();
    return;
  }

  if (paused_) {
    return;
  }

  if (activeBackend_ == PlayerBackendId::kAudioTools) {
    audioTools_.update();
    if (audioTools_.isActive()) {
      return;
    }

    if (repeatMode_ == RepeatMode::kAll && trackCount_ > 0U) {
      currentTrack_ = static_cast<uint16_t>((currentTrack_ + 1U) % trackCount_);
    }
    startCurrentTrack();
    return;
  }

  if (activeBackend_ == PlayerBackendId::kLegacy && decoder_ != nullptr) {
    if (decoder_->isRunning()) {
      if (decoder_->loop()) {
        return;
      }

      const String failedTrack = currentTrackName();
      Serial.printf("[MP3] Decoder loop stop [%s]: %s\n",
                    codecLabel(activeCodec_),
                    failedTrack.isEmpty() ? "-" : failedTrack.c_str());
    }

    stopLegacyTrack();
    if (repeatMode_ == RepeatMode::kAll && trackCount_ > 0U) {
      currentTrack_ = static_cast<uint16_t>((currentTrack_ + 1U) % trackCount_);
    }
    startCurrentTrack();
    return;
  }

  if (nowMs < nextRetryMs_) {
    return;
  }
  startCurrentTrack();
}

void Mp3Player::togglePause() {
  if (!sdReady_ || trackCount_ == 0U) {
    return;
  }
  paused_ = !paused_;
  markStateDirty();
}

void Mp3Player::restartTrack() {
  if (!sdReady_ || trackCount_ == 0U) {
    return;
  }
  paused_ = false;
  stop();
  startCurrentTrack();
}

void Mp3Player::nextTrack() {
  if (!sdReady_ || trackCount_ == 0U) {
    return;
  }

  paused_ = false;
  stop();
  currentTrack_ = static_cast<uint16_t>((currentTrack_ + 1U) % trackCount_);
  markStateDirty();
  startCurrentTrack();
}

void Mp3Player::previousTrack() {
  if (!sdReady_ || trackCount_ == 0U) {
    return;
  }

  paused_ = false;
  stop();
  if (currentTrack_ == 0U) {
    currentTrack_ = static_cast<uint16_t>(trackCount_ - 1U);
  } else {
    --currentTrack_;
  }
  markStateDirty();
  startCurrentTrack();
}

void Mp3Player::cycleRepeatMode() {
  repeatMode_ = (repeatMode_ == RepeatMode::kAll) ? RepeatMode::kOne : RepeatMode::kAll;
  markStateDirty();
}

void Mp3Player::requestStorageRefresh(bool forceRebuild) {
  forceRescan_ = forceRescan_ || forceRebuild;
  nextMountAttemptMs_ = 0;
  nextRescanMs_ = 0;
  requestCatalogScan(forceRebuild);
}

void Mp3Player::requestCatalogScan(bool forceRebuild) {
  if (!sdReady_) {
    forceRescan_ = forceRescan_ || forceRebuild;
    scanProgress_.pendingRequest = true;
    scanProgress_.forceRebuild = forceRescan_;
    setScanReason(&scanProgress_, "WAIT_SD");
    return;
  }
  scanService_.request(forceRebuild);
  scanProgress_.pendingRequest = true;
  scanProgress_.forceRebuild = forceRebuild;
  setScanReason(&scanProgress_, forceRebuild ? "REQ_REBUILD" : "REQ_SCAN");
}

bool Mp3Player::cancelCatalogScan() {
  const bool wasBusy = scanService_.isBusy();
  scanService_.cancel();
  clearScanContext();
  scanBusy_ = false;
  scanProgress_.active = false;
  scanProgress_.pendingRequest = false;
  scanProgress_.entriesThisTick = 0U;
  setScanReason(&scanProgress_, wasBusy ? "CANCELED" : "IDLE");
  return wasBusy;
}

const char* Mp3Player::scanStateLabel() const {
  return scanStateToLabel(scanService_.state());
}

void Mp3Player::setGain(float gain) {
  if (gain < 0.0f) {
    gain = 0.0f;
  } else if (gain > 1.0f) {
    gain = 1.0f;
  }

  gain_ = gain;
  if (i2sOut_ != nullptr) {
    i2sOut_->SetGain(gain_);
  }
  audioTools_.setGain(gain_);
  markStateDirty();
}

float Mp3Player::gain() const {
  return gain_;
}

uint8_t Mp3Player::volumePercent() const {
  return static_cast<uint8_t>(gain_ * 100.0f);
}

void Mp3Player::setFxMode(Mp3FxMode mode) {
  fxMode_ = mode;
  if (i2sOut_ != nullptr) {
    i2sOut_->setFxMode(mode);
  }
  markStateDirty();
}

Mp3FxMode Mp3Player::fxMode() const {
  return fxMode_;
}

const char* Mp3Player::fxModeLabel() const {
  return (fxMode_ == Mp3FxMode::kDucking) ? "DUCKING" : "OVERLAY";
}

void Mp3Player::setFxDuckingGain(float gain) {
  if (gain < 0.0f) {
    gain = 0.0f;
  } else if (gain > 1.0f) {
    gain = 1.0f;
  }
  fxDuckingGain_ = gain;
  if (i2sOut_ != nullptr) {
    i2sOut_->setDuckingGain(gain);
  }
}

float Mp3Player::fxDuckingGain() const {
  return fxDuckingGain_;
}

void Mp3Player::setFxOverlayGain(float gain) {
  if (gain < 0.0f) {
    gain = 0.0f;
  } else if (gain > 1.0f) {
    gain = 1.0f;
  }
  fxOverlayGain_ = gain;
  if (i2sOut_ != nullptr) {
    i2sOut_->setOverlayGain(gain);
  }
}

float Mp3Player::fxOverlayGain() const {
  return fxOverlayGain_;
}

bool Mp3Player::triggerFx(Mp3FxEffect effect, uint32_t durationMs) {
  fxLastEffect_ = effect;
  if (activeBackend_ != PlayerBackendId::kLegacy) {
    return false;
  }
  if (i2sOut_ == nullptr || decoder_ == nullptr || !decoder_->isRunning() || paused_) {
    return false;
  }
  return i2sOut_->triggerFx(effect, durationMs);
}

void Mp3Player::stopFx() {
  if (i2sOut_ != nullptr) {
    i2sOut_->stopFx();
  }
}

bool Mp3Player::isFxActive() const {
  return i2sOut_ != nullptr && i2sOut_->isFxActive();
}

uint32_t Mp3Player::fxRemainingMs() const {
  if (i2sOut_ == nullptr) {
    return 0U;
  }
  return i2sOut_->fxRemainingMs();
}

const char* Mp3Player::fxEffectLabel() const {
  const Mp3FxEffect effect =
      (i2sOut_ != nullptr && i2sOut_->isFxActive()) ? i2sOut_->activeFx() : fxLastEffect_;
  return audioEffectLabel(effect);
}

bool Mp3Player::isPaused() const {
  return paused_;
}

bool Mp3Player::isSdReady() const {
  return sdReady_;
}

bool Mp3Player::hasTracks() const {
  return trackCount_ > 0U;
}

bool Mp3Player::isPlaying() const {
  if (paused_) {
    return false;
  }
  if (activeBackend_ == PlayerBackendId::kAudioTools) {
    return audioTools_.isActive();
  }
  return decoder_ != nullptr && decoder_->isRunning();
}

uint16_t Mp3Player::trackCount() const {
  return trackCount_;
}

uint16_t Mp3Player::currentTrackNumber() const {
  if (trackCount_ == 0U) {
    return 0U;
  }
  return static_cast<uint16_t>(currentTrack_ + 1U);
}

String Mp3Player::currentTrackName() const {
  if (trackCount_ == 0U) {
    return String();
  }
  const TrackEntry* entry = catalog_.entry(currentTrack_);
  if (entry == nullptr) {
    return String();
  }
  return String(entry->path);
}

RepeatMode Mp3Player::repeatMode() const {
  return repeatMode_;
}

const char* Mp3Player::repeatModeLabel() const {
  return (repeatMode_ == RepeatMode::kAll) ? "ALL" : "ONE";
}

void Mp3Player::setBackendMode(PlayerBackendMode mode) {
  if (backendMode_ == mode) {
    return;
  }
  backendMode_ = mode;
  markStateDirty();
  if (isPlaying()) {
    restartTrack();
  }
}

PlayerBackendMode Mp3Player::backendMode() const {
  return backendMode_;
}

PlayerBackendId Mp3Player::activeBackend() const {
  return activeBackend_;
}

const char* Mp3Player::backendModeLabel() const {
  return playerBackendModeLabel(backendMode_);
}

const char* Mp3Player::activeBackendLabel() const {
  return playerBackendIdLabel(activeBackend_);
}

const char* Mp3Player::lastBackendError() const {
  return backendError_;
}

const char* Mp3Player::lastFallbackReason() const {
  return backendStats_.lastFallbackReason;
}

PlayerBackendCapabilities Mp3Player::audioToolsCapabilities() const {
  return audioTools_.capabilities();
}

PlayerBackendCapabilities Mp3Player::legacyCapabilities() const {
  PlayerBackendCapabilities caps;
  caps.mp3 = true;
  caps.wav = true;
  caps.aac = true;
  caps.flac = true;
  caps.opus = true;
  caps.supportsOverlayFx = true;
  return caps;
}

bool Mp3Player::backendSupportsCodec(PlayerBackendId backend, AudioCodec codec) const {
  if (backend == PlayerBackendId::kAudioTools) {
    return playerBackendSupportsCodec(audioTools_.capabilities(), codec);
  }
  if (backend == PlayerBackendId::kLegacy) {
    return playerBackendSupportsCodec(legacyCapabilities(), codec);
  }
  return false;
}

bool Mp3Player::selectTrackByIndex(uint16_t index, bool restart) {
  if (index >= trackCount_) {
    return false;
  }
  currentTrack_ = index;
  markStateDirty();
  if (restart) {
    restartTrack();
  }
  return true;
}

bool Mp3Player::selectTrackByPath(const char* path, bool restart) {
  const int16_t idx = catalog_.indexOfPath(path);
  if (idx < 0) {
    return false;
  }
  return selectTrackByIndex(static_cast<uint16_t>(idx), restart);
}

bool Mp3Player::playPath(const char* path) {
  return selectTrackByPath(path, true);
}

CatalogStats Mp3Player::catalogStats() const {
  return catalogStats_;
}

bool Mp3Player::isScanBusy() const {
  return scanBusy_ || scanService_.isBusy();
}

Mp3ScanProgress Mp3Player::scanProgress() const {
  return scanProgress_;
}

Mp3BackendRuntimeStats Mp3Player::backendStats() const {
  return backendStats_;
}

const TrackEntry* Mp3Player::trackEntryByNumber(uint16_t oneBasedNumber) const {
  if (oneBasedNumber == 0U) {
    return nullptr;
  }
  return catalog_.entry(static_cast<uint16_t>(oneBasedNumber - 1U));
}

uint16_t Mp3Player::listTracks(const char* prefix,
                               uint16_t offset,
                               uint16_t limit,
                               Print& out) const {
  return catalog_.listByPrefix(prefix, offset, limit, out);
}

uint16_t Mp3Player::countTracks(const char* prefix) const {
  return catalog_.countByPrefix(prefix);
}

bool Mp3Player::savePlayerState() {
  if (!sdReady_) {
    return false;
  }

  const String currentPath = currentTrackName();
  const String escapedPath = jsonEscape(currentPath);
  if (SD_MMC.exists(kStatePath)) {
    SD_MMC.remove(kStatePath);
  }
  fs::File file = SD_MMC.open(kStatePath, FILE_WRITE);
  if (!file || file.isDirectory()) {
    return false;
  }

  file.printf("{\"last_path\":\"%s\",\"volume\":%.3f,\"repeat\":\"%s\",\"backend_mode\":\"%s\",\"last_position_ms\":%lu}\n",
              escapedPath.c_str(),
              static_cast<double>(gain_),
              repeatModeToToken(repeatMode_),
              backendModeLabel(),
              static_cast<unsigned long>(lastPositionMs_));
  file.close();
  stateDirty_ = false;
  return true;
}

bool Mp3Player::loadPlayerState() {
  selectedPathFromState_.clear();
  fs::File file = SD_MMC.open(kStatePath, FILE_READ);
  if (!file || file.isDirectory()) {
    return false;
  }

  const String json = file.readString();
  file.close();

  String tmp;
  if (parseJsonString(json, "last_path", &tmp)) {
    selectedPathFromState_ = tmp;
  }

  float savedVolume = gain_;
  if (parseJsonFloat(json, "volume", &savedVolume)) {
    setGain(savedVolume);
  }

  if (parseJsonString(json, "repeat", &tmp)) {
    repeatMode_ = repeatModeFromToken(tmp.c_str());
  }

  if (parseJsonString(json, "backend_mode", &tmp)) {
    if (tmp == "AUDIO_TOOLS_ONLY") {
      backendMode_ = PlayerBackendMode::kAudioToolsOnly;
    } else if (tmp == "LEGACY_ONLY") {
      backendMode_ = PlayerBackendMode::kLegacyOnly;
    } else {
      backendMode_ = PlayerBackendMode::kAutoFallback;
    }
  }

  float savedPos = 0.0f;
  if (parseJsonFloat(json, "last_position_ms", &savedPos) && savedPos >= 0.0f) {
    lastPositionMs_ = static_cast<uint32_t>(savedPos);
  }
  return true;
}

bool Mp3Player::resetPlayerState() {
  selectedPathFromState_.clear();
  lastPositionMs_ = 0U;
  stateDirty_ = false;
  if (!sdReady_) {
    return false;
  }
  if (SD_MMC.exists(kStatePath)) {
    return SD_MMC.remove(kStatePath);
  }
  return true;
}

bool Mp3Player::mountStorage(uint32_t nowMs) {
  if (!SD_MMC.begin("/sdcard", true)) {
    nextMountAttemptMs_ = nowMs + 2000U;
    return false;
  }

  sdReady_ = true;
  nextCardCheckMs_ = nowMs + 1000U;
  nextRescanMs_ = nowMs;
  loadPlayerState();
  Serial.println("[MP3] SD_MMC mounted.");
  requestCatalogScan(forceRescan_);
  return true;
}

void Mp3Player::unmountStorage(uint32_t nowMs) {
  stop();
  SD_MMC.end();

  sdReady_ = false;
  paused_ = false;
  trackCount_ = 0U;
  currentTrack_ = 0U;
  nextMountAttemptMs_ = nowMs + 1500U;
  nextCardCheckMs_ = 0U;
  nextRescanMs_ = 0U;
  nextRetryMs_ = 0U;
  catalog_.clear();
  scanService_.reset();
  clearScanContext();
  scanBusy_ = false;
  scanProgress_ = Mp3ScanProgress();
  scanProgress_.tickBudgetMs = static_cast<uint16_t>(kScanTickBudgetMs);
  scanProgress_.tickEntryBudget = kScanTickEntryBudget;
  setScanReason(&scanProgress_, "UNMOUNTED");

  Serial.println("[MP3] SD removed/unmounted.");
}

void Mp3Player::refreshStorage(uint32_t nowMs) {
  if (!sdReady_) {
    if (nowMs >= nextMountAttemptMs_) {
      mountStorage(nowMs);
    }
    return;
  }

  if (nowMs >= nextCardCheckMs_) {
    nextCardCheckMs_ = nowMs + 1000U;
    if (SD_MMC.cardType() == CARD_NONE) {
      unmountStorage(nowMs);
      return;
    }
  }

  if (forceRescan_) {
    requestCatalogScan(true);
    forceRescan_ = false;
  }

  if (trackCount_ == 0U && nowMs >= nextRescanMs_ && !scanService_.isBusy()) {
    requestCatalogScan(false);
    nextRescanMs_ = nowMs + 3000U;
  }

  beginScanIfRequested(nowMs);
  updateScan(nowMs);

  if (trackCount_ > 0U && currentTrack_ >= trackCount_) {
    currentTrack_ = static_cast<uint16_t>(trackCount_ - 1U);
  }
}

void Mp3Player::beginScanIfRequested(uint32_t nowMs) {
  if (!scanService_.hasPendingRequest()) {
    scanProgress_.pendingRequest = false;
    return;
  }

  // Do not rebuild while an active stream is currently using the catalog.
  if (activeBackend_ != PlayerBackendId::kNone && trackCount_ > 0U) {
    setScanReason(&scanProgress_, "DEFER_PLAYING");
    return;
  }

  scanBusy_ = true;
  scanProgress_ = Mp3ScanProgress();
  scanProgress_.active = true;
  scanProgress_.pendingRequest = false;
  scanProgress_.forceRebuild = scanService_.forceRebuildRequested();
  scanProgress_.tickBudgetMs = static_cast<uint16_t>(kScanTickBudgetMs);
  scanProgress_.tickEntryBudget = kScanTickEntryBudget;
  setScanReason(&scanProgress_, "START");
  catalogStats_ = CatalogStats();
  clearScanContext();
  scanService_.start(nowMs);

  const bool forceRebuild = scanService_.forceRebuildRequested();
  bool loadedFromIndex = false;
  if (!forceRebuild) {
    loadedFromIndex = catalog_.loadIndex(SD_MMC, kIndexPath, &catalogStats_);
  }

  if (loadedFromIndex) {
    scanProgress_.tracksAccepted = catalog_.size();
    setScanReason(&scanProgress_, "INDEX_HIT");
    finalizeScan(nowMs, true, true);
    return;
  }

  catalog_.clear();
  trackCount_ = 0U;
  currentTrack_ = 0U;

  scanCtx_.active = true;
  setScanReason(&scanProgress_, forceRebuild ? "REBUILD" : "SCAN");
  if (!pushScanDir("/", 0U)) {
    setScanReason(&scanProgress_, "STACK_OVF");
    finalizeScan(nowMs, false, false);
  }
}

void Mp3Player::updateScan(uint32_t nowMs) {
  if (scanService_.state() != CatalogScanService::State::kRunning || !scanCtx_.active) {
    return;
  }

  const uint32_t budgetStartMs = millis();
  uint16_t entriesThisTick = 0U;
  while (static_cast<uint32_t>(millis() - budgetStartMs) < kScanTickBudgetMs &&
         entriesThisTick < kScanTickEntryBudget) {
    if (!scanCtx_.currentDir) {
      String dirPath;
      uint8_t depth = 0U;
      if (!popScanDir(&dirPath, &depth)) {
        setScanReason(&scanProgress_, "COMPLETE");
        finalizeScan(nowMs, true, false);
        return;
      }
      scanCtx_.currentDir = SD_MMC.open(dirPath.c_str());
      if (!scanCtx_.currentDir || !scanCtx_.currentDir.isDirectory()) {
        if (scanCtx_.currentDir) {
          scanCtx_.currentDir.close();
        }
        continue;
      }
      scanCtx_.currentDepth = depth;
      scanProgress_.depth = depth;
      scanProgress_.stackSize = scanCtx_.stackSize;
      ++catalogStats_.folders;
      ++scanProgress_.foldersScanned;
      setScanReason(&scanProgress_, "SCANNING");
    }

    fs::File entry = scanCtx_.currentDir.openNextFile();
    if (!entry) {
      scanCtx_.currentDir.close();
      continue;
    }

    String path = String(entry.name());
    const bool isDir = entry.isDirectory();
    const uint32_t fileSize = static_cast<uint32_t>(entry.size());
    entry.close();
    ++entriesThisTick;
    ++scanProgress_.filesScanned;

    if (!path.startsWith("/")) {
      path = "/" + path;
    }

    if (isDir) {
      if (scanCtx_.currentDepth < kScanMaxDepth) {
        if (!pushScanDir(path.c_str(), static_cast<uint8_t>(scanCtx_.currentDepth + 1U))) {
          Serial.printf("[MP3] Catalog scan queue overflow at '%s' (max=%u).\n",
                        path.c_str(),
                        static_cast<unsigned int>(kScanDirStackMax));
          setScanReason(&scanProgress_, "STACK_OVF");
          finalizeScan(nowMs, false, false);
          return;
        }
        scanProgress_.stackSize = scanCtx_.stackSize;
      }
      continue;
    }

    if (catalogCodecFromPath(path.c_str()) == CatalogCodec::kUnknown) {
      continue;
    }

    if (!catalog_.appendFallbackPath(path.c_str(), fileSize)) {
      scanCtx_.limitReached = true;
      scanProgress_.limitReached = true;
      setScanReason(&scanProgress_, "LIMIT");
      finalizeScan(nowMs, true, false);
      return;
    }
    scanProgress_.tracksAccepted = catalog_.size();
  }

  ++scanProgress_.ticks;
  scanProgress_.entriesThisTick = entriesThisTick;
  if (entriesThisTick >= kScanTickEntryBudget) {
    ++scanProgress_.entryBudgetHits;
  }
  const uint32_t startedAt = scanService_.startedAtMs();
  scanProgress_.elapsedMs = (startedAt == 0U || nowMs < startedAt) ? 0U : (nowMs - startedAt);
}

void Mp3Player::finalizeScan(uint32_t nowMs, bool success, bool loadedFromIndex) {
  const bool wasTruncated = scanCtx_.limitReached;
  const uint32_t startedAt = scanService_.startedAtMs();
  catalogStats_.scanMs = (startedAt == 0U || nowMs < startedAt) ? 0U : (nowMs - startedAt);
  scanProgress_.elapsedMs = catalogStats_.scanMs;
  scanProgress_.pendingRequest = false;
  scanProgress_.active = false;
  scanProgress_.entriesThisTick = 0U;

  if (!success) {
    scanBusy_ = false;
    scanService_.finish(CatalogScanService::State::kFailed, nowMs);
    clearScanContext();
    setScanReason(&scanProgress_, "FAILED");
    Serial.println("[MP3] Catalog scan failed.");
    return;
  }

  if (catalog_.size() == 0U && isSupportedAudioFile(String(mp3Path_)) && SD_MMC.exists(mp3Path_)) {
    uint32_t fallbackSize = 0U;
    fs::File f = SD_MMC.open(mp3Path_, FILE_READ);
    if (f) {
      fallbackSize = static_cast<uint32_t>(f.size());
      f.close();
    }
    catalog_.appendFallbackPath(mp3Path_, fallbackSize);
  }

  catalog_.sort();
  trackCount_ = catalog_.size();
  catalogStats_.tracks = trackCount_;
  catalogStats_.indexed = true;
  catalogStats_.metadataBestEffort = loadedFromIndex;
  if (!loadedFromIndex) {
    catalog_.saveIndex(SD_MMC, kIndexPath);
  }

  if (trackCount_ == 0U) {
    scanBusy_ = false;
    scanService_.finish(CatalogScanService::State::kDone, nowMs);
    clearScanContext();
    setScanReason(&scanProgress_, "EMPTY");
    Serial.println("[MP3] No supported audio file found on SD.");
    return;
  }

  if (currentTrack_ >= trackCount_) {
    currentTrack_ = 0U;
  }
  restoreTrackFromStatePath();

  scanBusy_ = false;
  scanProgress_.tracksAccepted = trackCount_;
  scanProgress_.limitReached = wasTruncated;
  if (loadedFromIndex) {
    setScanReason(&scanProgress_, "INDEX_HIT");
  } else if (wasTruncated) {
    setScanReason(&scanProgress_, "DONE_LIMIT");
  } else {
    setScanReason(&scanProgress_, "DONE");
  }
  scanService_.finish(CatalogScanService::State::kDone, nowMs);
  clearScanContext();
  Serial.printf("[MP3] %u track(s) loaded. index=%s%s\n",
                static_cast<unsigned int>(trackCount_),
                loadedFromIndex ? "HIT" : "REBUILD",
                wasTruncated ? " (TRUNCATED)" : "");
}

bool Mp3Player::pushScanDir(const char* path, uint8_t depth) {
  if (path == nullptr || path[0] == '\0' || scanCtx_.stackSize >= kScanDirStackMax) {
    return false;
  }
  const uint8_t idx = scanCtx_.stackSize;
  if (path[0] == '/') {
    snprintf(scanCtx_.stackPath[idx], sizeof(scanCtx_.stackPath[idx]), "%s", path);
  } else {
    snprintf(scanCtx_.stackPath[idx], sizeof(scanCtx_.stackPath[idx]), "/%s", path);
  }
  scanCtx_.stackDepth[idx] = depth;
  ++scanCtx_.stackSize;
  return true;
}

bool Mp3Player::popScanDir(String* outPath, uint8_t* outDepth) {
  if (scanCtx_.stackSize == 0U || outPath == nullptr || outDepth == nullptr) {
    return false;
  }
  --scanCtx_.stackSize;
  *outDepth = scanCtx_.stackDepth[scanCtx_.stackSize];
  *outPath = scanCtx_.stackPath[scanCtx_.stackSize];
  return true;
}

void Mp3Player::clearScanContext() {
  if (scanCtx_.currentDir) {
    scanCtx_.currentDir.close();
  }
  scanCtx_.active = false;
  scanCtx_.limitReached = false;
  scanCtx_.stackSize = 0U;
  scanCtx_.currentDepth = 0U;
}

void Mp3Player::updateDeferredStateSave(uint32_t nowMs) {
  if (!stateDirty_ || !sdReady_) {
    return;
  }
  if (static_cast<int32_t>(nowMs - nextStateSaveMs_) < 0) {
    return;
  }
  savePlayerState();
}

bool Mp3Player::startLegacyTrack() {
  ++backendStats_.startAttempts;
  ++backendStats_.legacyAttempts;
  if (!sdReady_ || trackCount_ == 0U || currentTrack_ >= trackCount_) {
    ++backendStats_.startFailures;
<<<<<<< HEAD
    ++backendStats_.legacyFailures;
    copyCStr(backendError_, sizeof(backendError_), "OUT_OF_CONTEXT");
=======
    setBackendReason(&backendStats_, "NO_TRACK");
>>>>>>> feature/MPRC-RC1-mp3-audio
    return false;
  }

  const TrackEntry* entry = catalog_.entry(currentTrack_);
  if (entry == nullptr || entry->path[0] == '\0') {
    ++backendStats_.startFailures;
<<<<<<< HEAD
    ++backendStats_.legacyFailures;
    copyCStr(backendError_, sizeof(backendError_), "BAD_PATH");
=======
    setBackendReason(&backendStats_, "NO_ENTRY");
>>>>>>> feature/MPRC-RC1-mp3-audio
    return false;
  }

  const String trackPath = String(entry->path);
  const AudioCodec trackCodec = codecForPath(trackPath);
  if (trackCodec == AudioCodec::kUnknown) {
    Serial.printf("[MP3] Unsupported file type: %s\n", trackPath.c_str());
    nextRetryMs_ = millis() + 250U;
    ++backendStats_.retriesScheduled;
    ++backendStats_.legacyRetries;
    ++backendStats_.startFailures;
<<<<<<< HEAD
    ++backendStats_.legacyFailures;
    copyCStr(backendError_, sizeof(backendError_), "UNSUPPORTED_CODEC");
=======
    setBackendReason(&backendStats_, "UNSUPPORTED_CODEC");
>>>>>>> feature/MPRC-RC1-mp3-audio
    return false;
  }

  if (!SD_MMC.exists(trackPath.c_str())) {
    Serial.printf("[MP3] Missing track: %s\n", trackPath.c_str());
    requestCatalogScan(true);
    nextRetryMs_ = millis() + 1000U;
    ++backendStats_.retriesScheduled;
    ++backendStats_.legacyRetries;
    ++backendStats_.startFailures;
<<<<<<< HEAD
    ++backendStats_.legacyFailures;
    copyCStr(backendError_, sizeof(backendError_), "OPEN_FAIL");
=======
    setBackendReason(&backendStats_, "MISSING_FILE");
>>>>>>> feature/MPRC-RC1-mp3-audio
    return false;
  }

  mp3File_ = new (std::nothrow) AudioFileSourceFS(SD_MMC, trackPath.c_str());
  i2sOut_ = new (std::nothrow) Mp3FxOverlayOutput();
  decoder_ = createDecoder(trackCodec);
  activeCodec_ = trackCodec;
  if (mp3File_ == nullptr || i2sOut_ == nullptr || decoder_ == nullptr) {
    Serial.println("[MP3] Memory allocation failed.");
    stopLegacyTrack();
    nextRetryMs_ = millis() + 1000U;
    ++backendStats_.retriesScheduled;
    ++backendStats_.legacyRetries;
    ++backendStats_.startFailures;
<<<<<<< HEAD
    ++backendStats_.legacyFailures;
    copyCStr(backendError_, sizeof(backendError_), "OOM");
=======
    setBackendReason(&backendStats_, "ALLOC_FAIL");
>>>>>>> feature/MPRC-RC1-mp3-audio
    return false;
  }

  i2sOut_->SetPinout(i2sBclk_, i2sLrc_, i2sDout_);
  i2sOut_->SetGain(gain_);
  i2sOut_->setFxMode(fxMode_);
  i2sOut_->setDuckingGain(fxDuckingGain_);
  i2sOut_->setOverlayGain(fxOverlayGain_);
  if (!decoder_->begin(mp3File_, i2sOut_)) {
    Serial.printf("[MP3] Unable to start %s playback.\n", codecLabel(trackCodec));
    stopLegacyTrack();
    nextRetryMs_ = millis() + 1000U;
    ++backendStats_.retriesScheduled;
    ++backendStats_.legacyRetries;
    ++backendStats_.startFailures;
<<<<<<< HEAD
    ++backendStats_.legacyFailures;
    copyCStr(backendError_, sizeof(backendError_), "DECODER_INIT_FAIL");
=======
    setBackendReason(&backendStats_, "DECODER_BEGIN_FAIL");
>>>>>>> feature/MPRC-RC1-mp3-audio
    return false;
  }

  activeBackend_ = PlayerBackendId::kLegacy;
  copyCStr(backendError_, sizeof(backendError_), "OK");
  ++backendStats_.startSuccess;
  ++backendStats_.legacyStarts;
<<<<<<< HEAD
  ++backendStats_.legacySuccess;
=======
  setBackendReason(&backendStats_, "OK");
>>>>>>> feature/MPRC-RC1-mp3-audio
  Serial.printf("[MP3] Playing %u/%u [%s|LEGACY]: %s\n",
                static_cast<unsigned int>(currentTrack_ + 1U),
                static_cast<unsigned int>(trackCount_),
                codecLabel(trackCodec),
                trackPath.c_str());
  return true;
}

bool Mp3Player::startAudioToolsTrack() {
  ++backendStats_.startAttempts;
  ++backendStats_.audioToolsAttempts;
  if (!sdReady_ || trackCount_ == 0U || currentTrack_ >= trackCount_) {
    ++backendStats_.startFailures;
<<<<<<< HEAD
    ++backendStats_.audioToolsFailures;
    copyCStr(backendError_, sizeof(backendError_), "OUT_OF_CONTEXT");
=======
    setBackendReason(&backendStats_, "NO_TRACK");
>>>>>>> feature/MPRC-RC1-mp3-audio
    return false;
  }
  const TrackEntry* entry = catalog_.entry(currentTrack_);
  if (entry == nullptr || entry->path[0] == '\0') {
    ++backendStats_.startFailures;
<<<<<<< HEAD
    ++backendStats_.audioToolsFailures;
    copyCStr(backendError_, sizeof(backendError_), "BAD_PATH");
=======
    setBackendReason(&backendStats_, "NO_ENTRY");
>>>>>>> feature/MPRC-RC1-mp3-audio
    return false;
  }
  const AudioCodec trackCodec = codecForPath(String(entry->path));
  if (!audioTools_.supportsCodec(trackCodec)) {
    copyCStr(backendError_, sizeof(backendError_), "UNSUPPORTED_CODEC");
    ++backendStats_.startFailures;
<<<<<<< HEAD
    ++backendStats_.audioToolsFailures;
=======
    setBackendReason(&backendStats_, "AT_UNSUPPORTED");
>>>>>>> feature/MPRC-RC1-mp3-audio
    return false;
  }

  if (!audioTools_.start(entry->path, gain_)) {
    copyCStr(backendError_, sizeof(backendError_), audioTools_.lastError());
    ++backendStats_.startFailures;
<<<<<<< HEAD
    ++backendStats_.audioToolsFailures;
=======
    setBackendReason(&backendStats_, audioTools_.lastError());
>>>>>>> feature/MPRC-RC1-mp3-audio
    return false;
  }

  activeBackend_ = PlayerBackendId::kAudioTools;
  activeCodec_ = trackCodec;
  copyCStr(backendError_, sizeof(backendError_), "OK");
  ++backendStats_.startSuccess;
  ++backendStats_.audioToolsStarts;
<<<<<<< HEAD
  ++backendStats_.audioToolsSuccess;
=======
  setBackendReason(&backendStats_, "OK");
>>>>>>> feature/MPRC-RC1-mp3-audio
  Serial.printf("[MP3] Playing %u/%u [%s|AUDIO_TOOLS]: %s\n",
                static_cast<unsigned int>(currentTrack_ + 1U),
                static_cast<unsigned int>(trackCount_),
                codecLabel(activeCodec_),
                entry->path);
  return true;
}

void Mp3Player::startCurrentTrack() {
  stop();

  if (!sdReady_ || trackCount_ == 0U || currentTrack_ >= trackCount_) {
    return;
  }

  fallbackUsed_ = false;
<<<<<<< HEAD
  setFallbackReason(&backendStats_, "NONE");
=======
  setFallbackPath(&backendStats_, "NONE");
>>>>>>> feature/MPRC-RC1-mp3-audio
  bool started = false;
  bool attemptedLegacy = false;
  bool attemptedTools = false;

  if (backendMode_ != PlayerBackendMode::kLegacyOnly) {
    attemptedTools = true;
    started = startAudioToolsTrack();
    if (!started && backendMode_ == PlayerBackendMode::kAutoFallback) {
      fallbackUsed_ = true;
      ++backendStats_.fallbackCount;
<<<<<<< HEAD
      setFallbackReason(&backendStats_, backendError_);
      attemptedLegacy = true;
=======
      setFallbackPath(&backendStats_, "AT->LEGACY");
>>>>>>> feature/MPRC-RC1-mp3-audio
      started = startLegacyTrack();
      if (!started) {
        setFallbackPath(&backendStats_, "AT->LEGACY_FAIL");
      }
    }
  } else {
    attemptedLegacy = true;
    started = startLegacyTrack();
  }

  if (!started && backendMode_ == PlayerBackendMode::kAudioToolsOnly) {
    nextRetryMs_ = millis() + 1000U;
    ++backendStats_.retriesScheduled;
    if (attemptedTools) {
      ++backendStats_.audioToolsRetries;
    }
    Serial.printf("[MP3] AudioTools start failed (mode=%s err=%s).\n",
                  backendModeLabel(),
                  backendError_);
    return;
  }

  if (!started) {
    nextRetryMs_ = millis() + 1000U;
    ++backendStats_.retriesScheduled;
<<<<<<< HEAD
    if (attemptedLegacy) {
      ++backendStats_.legacyRetries;
    } else if (attemptedTools) {
      ++backendStats_.audioToolsRetries;
=======
    if (backendError_[0] == '\0') {
      copyCStr(backendError_, sizeof(backendError_), "START_FAIL");
>>>>>>> feature/MPRC-RC1-mp3-audio
    }
    return;
  }

  if (fallbackUsed_) {
    Serial.printf("[MP3] Backend fallback AUDIO_TOOLS->LEGACY active.\n");
  }
}

void Mp3Player::stopLegacyTrack() {
  if (decoder_ != nullptr) {
    if (decoder_->isRunning()) {
      decoder_->stop();
    }
    delete decoder_;
    decoder_ = nullptr;
  }

  if (mp3File_ != nullptr) {
    delete mp3File_;
    mp3File_ = nullptr;
  }

  if (i2sOut_ != nullptr) {
    delete i2sOut_;
    i2sOut_ = nullptr;
  }
}

void Mp3Player::stopAudioToolsTrack() {
  audioTools_.stop();
}

void Mp3Player::stop() {
  stopLegacyTrack();
  stopAudioToolsTrack();
  activeBackend_ = PlayerBackendId::kNone;
  activeCodec_ = AudioCodec::kUnknown;
}

void Mp3Player::markStateDirty() {
  stateDirty_ = true;
  nextStateSaveMs_ = millis() + kStateSaveDebounceMs;
}

void Mp3Player::syncCurrentTrackToStatePath() {
  selectedPathFromState_ = currentTrackName();
}

bool Mp3Player::restoreTrackFromStatePath() {
  if (selectedPathFromState_.isEmpty()) {
    return false;
  }
  const int16_t idx = catalog_.indexOfPath(selectedPathFromState_.c_str());
  if (idx < 0) {
    return false;
  }
  currentTrack_ = static_cast<uint16_t>(idx);
  return true;
}

bool Mp3Player::isSupportedAudioFile(const String& filename) {
  return codecForPath(filename) != AudioCodec::kUnknown;
}

AudioCodec Mp3Player::codecForPath(const String& filename) {
  String lower = filename;
  lower.toLowerCase();
  if (lower.endsWith(".mp3")) {
    return AudioCodec::kMp3;
  }
  if (lower.endsWith(".wav")) {
    return AudioCodec::kWav;
  }
  if (lower.endsWith(".aac") || lower.endsWith(".m4a")) {
    return AudioCodec::kAac;
  }
  if (lower.endsWith(".flac")) {
    return AudioCodec::kFlac;
  }
  if (lower.endsWith(".opus") || lower.endsWith(".ogg")) {
    return AudioCodec::kOpus;
  }
  return AudioCodec::kUnknown;
}

const char* Mp3Player::codecLabel(AudioCodec codec) {
  switch (codec) {
    case AudioCodec::kMp3:
      return "MP3";
    case AudioCodec::kWav:
      return "WAV";
    case AudioCodec::kAac:
      return "AAC";
    case AudioCodec::kFlac:
      return "FLAC";
    case AudioCodec::kOpus:
      return "OPUS";
    case AudioCodec::kUnknown:
    default:
      return "UNKNOWN";
  }
}

AudioGenerator* Mp3Player::createDecoder(AudioCodec codec) {
  switch (codec) {
    case AudioCodec::kMp3:
      return new (std::nothrow) AudioGeneratorMP3();
    case AudioCodec::kWav:
      return new (std::nothrow) AudioGeneratorWAV();
    case AudioCodec::kAac:
      return new (std::nothrow) AudioGeneratorAAC();
    case AudioCodec::kFlac:
      return new (std::nothrow) AudioGeneratorFLAC();
    case AudioCodec::kOpus:
      return new (std::nothrow) AudioGeneratorOpus();
    case AudioCodec::kUnknown:
    default:
      return nullptr;
  }
}

const char* Mp3Player::repeatModeToToken(RepeatMode mode) {
  return (mode == RepeatMode::kOne) ? "ONE" : "ALL";
}

RepeatMode Mp3Player::repeatModeFromToken(const char* token) {
  if (token == nullptr) {
    return RepeatMode::kAll;
  }
  if (strcasecmp(token, "ONE") == 0) {
    return RepeatMode::kOne;
  }
  return RepeatMode::kAll;
}

bool Mp3Player::parseJsonString(const String& json, const char* key, String* outValue) {
  if (outValue == nullptr || key == nullptr || key[0] == '\0') {
    return false;
  }
  const String token = String("\"") + key + "\"";
  const int keyPos = json.indexOf(token);
  if (keyPos < 0) {
    return false;
  }
  const int colonPos = json.indexOf(':', keyPos + token.length());
  if (colonPos < 0) {
    return false;
  }
  int firstQuote = json.indexOf('"', colonPos + 1);
  if (firstQuote < 0) {
    return false;
  }
  int secondQuote = json.indexOf('"', firstQuote + 1);
  if (secondQuote < 0) {
    return false;
  }
  *outValue = json.substring(firstQuote + 1, secondQuote);
  return true;
}

bool Mp3Player::parseJsonFloat(const String& json, const char* key, float* outValue) {
  if (outValue == nullptr || key == nullptr || key[0] == '\0') {
    return false;
  }
  const String token = String("\"") + key + "\"";
  const int keyPos = json.indexOf(token);
  if (keyPos < 0) {
    return false;
  }
  const int colonPos = json.indexOf(':', keyPos + token.length());
  if (colonPos < 0) {
    return false;
  }
  int valueStart = colonPos + 1;
  while (valueStart < static_cast<int>(json.length()) &&
         (json[valueStart] == ' ' || json[valueStart] == '\t')) {
    ++valueStart;
  }
  int valueEnd = valueStart;
  while (valueEnd < static_cast<int>(json.length())) {
    const char c = json[valueEnd];
    if ((c >= '0' && c <= '9') || c == '.' || c == '-' || c == '+') {
      ++valueEnd;
      continue;
    }
    break;
  }
  if (valueEnd <= valueStart) {
    return false;
  }
  *outValue = json.substring(valueStart, valueEnd).toFloat();
  return true;
}
