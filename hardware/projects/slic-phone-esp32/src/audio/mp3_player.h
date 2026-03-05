#pragma once

#include <Arduino.h>
#include <FS.h>

#include "catalog/track_catalog.h"
#include "mp3_fx_overlay_output.h"
#include "player/audio_tools_backend.h"
#include "player/player_backend.h"
#include "../services/storage/catalog_scan_service.h"

class AudioFileSourceFS;
class AudioGenerator;

enum class AudioCodec : uint8_t {
  kUnknown = 0,
  kMp3,
  kWav,
  kAac,
  kFlac,
  kOpus,
};

enum class RepeatMode : uint8_t {
  kAll = 0,
  kOne = 1,
};

struct Mp3ScanProgress {
  bool active = false;
  bool pendingRequest = false;
  bool forceRebuild = false;
  bool limitReached = false;
  uint8_t depth = 0U;
  uint8_t stackSize = 0U;
  uint16_t foldersScanned = 0U;
  uint16_t filesScanned = 0U;
  uint16_t tracksAccepted = 0U;
  uint16_t entriesThisTick = 0U;
  uint16_t entryBudgetHits = 0U;
  uint32_t ticks = 0U;
  uint32_t elapsedMs = 0U;
  uint16_t tickBudgetMs = 0U;
  uint16_t tickEntryBudget = 0U;
  char reason[24] = "IDLE";
};

struct Mp3BackendRuntimeStats {
  uint32_t startAttempts = 0U;
  uint32_t startSuccess = 0U;
  uint32_t startFailures = 0U;
  uint32_t retriesScheduled = 0U;
  uint32_t fallbackCount = 0U;
  uint32_t legacyStarts = 0U;
  uint32_t audioToolsStarts = 0U;
  char lastFailureReason[24] = "OK";
  char lastFallbackPath[24] = "NONE";
};

class Mp3Player {
 public:
  Mp3Player(uint8_t i2sBclk,
            uint8_t i2sLrc,
            uint8_t i2sDout,
            const char* mp3Path,
            int8_t paEnablePin = -1);
  ~Mp3Player();

  void begin();
  void update(uint32_t nowMs, bool allowPlayback = true);
  void togglePause();
  void restartTrack();
  void nextTrack();
  void previousTrack();
  void cycleRepeatMode();
  void requestStorageRefresh(bool forceRebuild = false);
  void requestCatalogScan(bool forceRebuild);
  bool cancelCatalogScan();
  const char* scanStateLabel() const;
  void setGain(float gain);
  float gain() const;
  uint8_t volumePercent() const;
  void setFxMode(Mp3FxMode mode);
  Mp3FxMode fxMode() const;
  const char* fxModeLabel() const;
  void setFxDuckingGain(float gain);
  float fxDuckingGain() const;
  void setFxOverlayGain(float gain);
  float fxOverlayGain() const;
  bool triggerFx(Mp3FxEffect effect, uint32_t durationMs);
  void stopFx();
  bool isFxActive() const;
  uint32_t fxRemainingMs() const;
  const char* fxEffectLabel() const;
  bool isPaused() const;
  bool isSdReady() const;
  bool hasTracks() const;
  bool isPlaying() const;
  uint16_t trackCount() const;
  uint16_t currentTrackNumber() const;
  String currentTrackName() const;
  RepeatMode repeatMode() const;
  const char* repeatModeLabel() const;

  void setBackendMode(PlayerBackendMode mode);
  PlayerBackendMode backendMode() const;
  PlayerBackendId activeBackend() const;
  const char* backendModeLabel() const;
  const char* activeBackendLabel() const;
  const char* lastBackendError() const;

  bool selectTrackByIndex(uint16_t index, bool restart = true);
  bool selectTrackByPath(const char* path, bool restart = true);
  bool playPath(const char* path);

  CatalogStats catalogStats() const;
  bool isScanBusy() const;
  Mp3ScanProgress scanProgress() const;
  Mp3BackendRuntimeStats backendStats() const;
  const TrackEntry* trackEntryByNumber(uint16_t oneBasedNumber) const;
  uint16_t listTracks(const char* prefix,
                      uint16_t offset,
                      uint16_t limit,
                      Print& out) const;
  uint16_t countTracks(const char* prefix) const;

  bool savePlayerState();
  bool loadPlayerState();
  bool resetPlayerState();

 private:
  static constexpr uint16_t kStateSaveDebounceMs = 1200;
  static constexpr uint8_t kScanDirStackMax = 24;

  bool mountStorage(uint32_t nowMs);
  void unmountStorage(uint32_t nowMs);
  void refreshStorage(uint32_t nowMs);
  void beginScanIfRequested(uint32_t nowMs);
  void updateScan(uint32_t nowMs);
  void finalizeScan(uint32_t nowMs, bool success, bool loadedFromIndex);
  bool pushScanDir(const char* path, uint8_t depth);
  bool popScanDir(String* outPath, uint8_t* outDepth);
  void clearScanContext();
  void updateDeferredStateSave(uint32_t nowMs);
  bool startLegacyTrack();
  bool startAudioToolsTrack();
  void stopLegacyTrack();
  void stopAudioToolsTrack();
  void markStateDirty();
  void syncCurrentTrackToStatePath();
  bool restoreTrackFromStatePath();
  static bool isSupportedAudioFile(const String& filename);
  static AudioCodec codecForPath(const String& filename);
  static const char* codecLabel(AudioCodec codec);
  static AudioGenerator* createDecoder(AudioCodec codec);
  static const char* repeatModeToToken(RepeatMode mode);
  static RepeatMode repeatModeFromToken(const char* token);
  static bool parseJsonString(const String& json, const char* key, String* outValue);
  static bool parseJsonFloat(const String& json, const char* key, float* outValue);

  void startCurrentTrack();
  void stop();

  uint8_t i2sBclk_;
  uint8_t i2sLrc_;
  uint8_t i2sDout_;
  int8_t paEnablePin_;
  const char* mp3Path_;

  bool sdReady_ = false;
  bool paused_ = false;
  float gain_ = 0.20f;
  uint32_t nextMountAttemptMs_ = 0;
  uint32_t nextCardCheckMs_ = 0;
  uint32_t nextRescanMs_ = 0;
  uint32_t nextRetryMs_ = 0;
  uint16_t trackCount_ = 0;
  uint16_t currentTrack_ = 0;
  String selectedPathFromState_;
  RepeatMode repeatMode_ = RepeatMode::kAll;
  bool forceRescan_ = false;
  bool scanBusy_ = false;
  Mp3ScanProgress scanProgress_;
  CatalogStats catalogStats_;
  TrackCatalog catalog_;
  CatalogScanService scanService_;
  struct ScanContext {
    bool active = false;
    bool limitReached = false;
    uint8_t stackSize = 0U;
    uint8_t currentDepth = 0U;
    char stackPath[kScanDirStackMax][120] = {};
    uint8_t stackDepth[kScanDirStackMax] = {};
    fs::File currentDir;
  } scanCtx_;
  AudioCodec activeCodec_ = AudioCodec::kUnknown;
  bool stateDirty_ = false;
  uint32_t nextStateSaveMs_ = 0;
  uint32_t lastPositionMs_ = 0;

  PlayerBackendMode backendMode_ = PlayerBackendMode::kAutoFallback;
  PlayerBackendId activeBackend_ = PlayerBackendId::kNone;
  bool fallbackUsed_ = false;
  char backendError_[24] = "OK";
  Mp3BackendRuntimeStats backendStats_;
  AudioToolsBackend audioTools_;

  Mp3FxMode fxMode_ = Mp3FxMode::kDucking;
  float fxDuckingGain_ = 0.45f;
  float fxOverlayGain_ = 0.42f;
  Mp3FxEffect fxLastEffect_ = Mp3FxEffect::kFmSweep;
  AudioGenerator* decoder_ = nullptr;
  AudioFileSourceFS* mp3File_ = nullptr;
  Mp3FxOverlayOutput* i2sOut_ = nullptr;
};
