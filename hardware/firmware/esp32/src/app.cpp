#include <Arduino.h>
#include <cstring>
#include <cmath>
#include <cctype>
#include <cstdio>

#include <AudioFileSourceFS.h>
#include <AudioGenerator.h>
#include <AudioGeneratorAAC.h>
#include <AudioGeneratorFLAC.h>
#include <AudioGeneratorMP3.h>
#include <AudioGeneratorOpus.h>
#include <AudioGeneratorWAV.h>
#include <AudioOutputI2S.h>
#include <FS.h>
#include <LittleFS.h>
#include <SD_MMC.h>

#include "app.h"
#include "audio/effects/audio_effect_id.h"
#include "audio/fm_radio_scan_fx.h"
#include "controllers/boot_protocol_controller.h"
#include "controllers/story_controller.h"
#include "runtime/app_scheduler.h"
#include "runtime/runtime_state.h"
#include "services/audio/audio_service.h"
#include "services/input/input_service.h"
#include "services/serial/serial_router.h"
#include "story/story_engine.h"
#include "ui/player_ui_model.h"

constexpr char kUnlockJingleRtttl[] =
    "zac_unlock:d=16,o=6,b=118:e,p,b,p,e7,8p,e7,b,e7";
constexpr uint32_t kBootLoopScanMinMs = 10000U;
constexpr uint32_t kBootLoopScanMaxMs = 40000U;
constexpr uint32_t kULockSearchSonarCueMs = 420U;
constexpr uint32_t kFxWinDurationMs = 1800U;
constexpr uint32_t kFxMorseDurationMs = 3200U;
constexpr uint32_t kFxSonarDurationMs = 2600U;
constexpr uint32_t kFxFmDurationMs = 2600U;

struct BootRadioScanState {
  bool restoreMicCapture = false;
  uint32_t lastLogMs = 0;
};

BootRadioScanState g_bootRadioScan;
FmRadioScanFx g_bootRadioScanFx(config::kPinI2SBclk,
                                config::kPinI2SLrc,
                                config::kPinI2SDout,
                                config::kI2sOutputPort);

StoryEngine::Options makeStoryOptions() {
  StoryEngine::Options options;
  options.etape2DelayMs = config::kStoryEtape2DelayMs;
  options.etape2TestDelayMs = config::kStoryEtape2TestDelayMs;
  return options;
}
StoryEngine g_story(makeStoryOptions());

struct ULockSearchAudioCueState {
  bool pending = false;
  bool active = false;
  bool restoreMicCapture = false;
  uint32_t untilMs = 0;
};

ULockSearchAudioCueState g_uLockSearchAudioCue;
uint32_t g_screenFrameSeq = 0U;
PlayerUiModel g_playerUi;
String g_mp3BrowsePath = "/";

void setBootAudioPaEnabled(bool enabled, const char* source);
void printBootAudioOutputInfo(const char* source);
void printLittleFsInfo(const char* source);
void listLittleFsRoot(const char* source);
void setupInternalLittleFs();
bool playAudioFromFsBlocking(fs::FS& storage,
                             const char* path,
                             float gain,
                             uint32_t maxDurationMs,
                             const char* source);
bool resolveBootLittleFsPath(String* outPath);
bool playBootLittleFsFx(const char* source);
void playBootAudioPrimaryFx(const char* source);
bool resolveRandomFsPathContaining(fs::FS& storage, const char* token, String* outPath);
bool playRandomLittleFsTokenFx(const char* token,
                               const char* source,
                               float gain,
                               uint32_t maxDurationMs,
                               String* outPath = nullptr);
bool playRandomTokenFx(const char* token,
                       const char* source,
                       bool allowSdFallback,
                       uint32_t maxDurationMs = config::kBootFxLittleFsMaxDurationMs);
void playRtttlJingleBlocking(const char* song, float gain, const char* source);
const char* effectLabel(FmRadioScanFx::Effect effect);
bool playGeneratedI2sFxBlocking(FmRadioScanFx::Effect effect,
                                uint32_t durationMs,
                                float gain,
                                const char* source);
bool startAudioFromFsAsync(fs::FS& storage,
                           const char* path,
                           float gain,
                           uint32_t maxDurationMs,
                           const char* source);
bool startBootAudioPrimaryFxAsync(const char* source);
bool startRandomTokenFxAsync(const char* token,
                             const char* source,
                             bool allowSdFallback,
                             uint32_t maxDurationMs = config::kBootFxLittleFsMaxDurationMs);
void updateAsyncAudioService(uint32_t nowMs);
void resetStoryTimeline(const char* source);
void armStoryTimelineAfterUnlock(uint32_t nowMs);
bool isMp3GateOpen();
void updateStoryTimeline(uint32_t nowMs);
bool startBootRadioScan(const char* source);
void stopBootRadioScan(const char* source);
void updateBootRadioScan(uint32_t nowMs);
void continueAfterBootProtocol(const char* source);
void startBootAudioLoopCycle(uint32_t nowMs, const char* source);
void startBootAudioValidationProtocol(uint32_t nowMs);
void updateBootAudioValidationProtocol(uint32_t nowMs);
void startMicCalibration(uint32_t nowMs, const char* reason);
bool processCodecDebugCommand(const char* cmd);
void printCodecDebugHelp();
void printMp3DebugHelp();
bool processMp3DebugCommand(const char* cmd, uint32_t nowMs);
void printStoryDebugHelp();
bool processStoryDebugCommand(const char* cmd, uint32_t nowMs);
void updateMp3FormatTest(uint32_t nowMs);
PlayerUiPage currentPlayerUiPage();
bool setPlayerUiPage(PlayerUiPage page);
bool parsePlayerUiPageToken(const char* token, PlayerUiPage* outPage);
uint8_t encodeBackendForScreen();
uint8_t encodeMp3ErrorForScreen();
void printMp3ScanStatus(const char* source);
void printMp3BrowseList(const char* source, const char* path, uint16_t offset, uint16_t limit);
bool parseBackendModeToken(const char* token, PlayerBackendMode* outMode);
const char* currentBrowsePath();
const char* mp3FxModeLabel(Mp3FxMode mode);
const char* mp3FxEffectLabel(Mp3FxEffect effect);
bool parseMp3FxEffectToken(const char* token, Mp3FxEffect* outEffect);
bool triggerMp3Fx(Mp3FxEffect effect, uint32_t durationMs, const char* source);
void handleBootAudioProtocolKey(uint8_t key, uint32_t nowMs);
void handleKeySelfTestPress(uint8_t key, uint16_t raw);
void handleKeyPress(uint8_t key);
bool serviceInputDuringBlockingAudio(uint32_t nowMs, const char* source);
void sendScreenFrameSnapshot(uint32_t nowMs, uint8_t keyForScreen);
void requestULockSearchSonarCue(const char* source);
void cancelULockSearchSonarCue(const char* source);
void serviceULockSearchSonarCue(uint32_t nowMs);
void onSerialCommand(const char* cmd, uint32_t nowMs, void* ctx);
bool isCanonicalSerialCommand(const char* cmd);
bool commandMatches(const char* cmd, const char* token);

InputService& inputService() {
  static InputService service(g_keypad);
  return service;
}

AudioService& audioService() {
  static AudioService service(g_asyncAudio, g_bootRadioScanFx, g_mp3);
  return service;
}

bool startStoryRandomTokenBaseHook(const char* token,
                                   const char* source,
                                   bool allowSdFallback,
                                   uint32_t maxDurationMs) {
  return startRandomTokenFxAsync(token, source, allowSdFallback, maxDurationMs);
}

bool startStoryFallbackBaseFxHook(AudioEffectId effect,
                                  uint32_t durationMs,
                                  float gain,
                                  const char* source) {
  return audioService().startBaseFx(effect, gain, durationMs, source);
}

StoryController& storyController() {
  static StoryController::Hooks hooks = []() {
    StoryController::Hooks out;
    out.startRandomTokenBase = startStoryRandomTokenBaseHook;
    out.startFallbackBaseFx = startStoryFallbackBaseFxHook;
    out.fallbackGain = config::kUnlockI2sJingleGain;
    out.winToken = "WIN";
    out.etape2Token = "ETAPE_2";
    out.winMaxDurationMs = 6000U;
    out.etape2MaxDurationMs = 6000U;
    out.winFallbackDurationMs = kFxWinDurationMs;
    out.etape2FallbackDurationMs = kFxWinDurationMs;
    return out;
  }();
  static StoryController controller(g_story, audioService(), hooks);
  return controller;
}

bool bootControllerIsActiveHook() {
  return g_bootAudioProtocol.active;
}

BootProtocolController& bootProtocolController() {
  static BootProtocolController::Hooks hooks = []() {
    BootProtocolController::Hooks out;
    out.start = startBootAudioValidationProtocol;
    out.update = updateBootAudioValidationProtocol;
    out.onKey = handleBootAudioProtocolKey;
    out.isActive = bootControllerIsActiveHook;
    return out;
  }();
  static BootProtocolController controller(hooks);
  return controller;
}

SerialRouter& serialRouter() {
  static SerialRouter router(Serial);
  return router;
}

const char* micHealthLabel(bool detectionEnabled, float micRms, uint16_t micMin, uint16_t micMax) {
  if (!detectionEnabled) {
    return "DETECT_OFF";
  }
  if (micMin <= 5 || micMax >= 4090) {
    return "SATURATION";
  }

  const uint16_t p2p = static_cast<uint16_t>(micMax - micMin);
  if (p2p < 12 || micRms < 2.0f) {
    return "SILENCE/GAIN";
  }
  if (micRms > 900.0f) {
    return "TOO_LOUD";
  }
  return "OK";
}

uint8_t micLevelPercentFromRms(float micRms) {
  const float fullScale = config::kMicRmsForScreenFullScale;
  if (fullScale <= 0.0f || micRms <= 0.0f) {
    return 0;
  }

  float percent = (micRms * 100.0f) / fullScale;
  if (percent < 0.0f) {
    percent = 0.0f;
  } else if (percent > 100.0f) {
    percent = 100.0f;
  }
  return static_cast<uint8_t>(percent);
}

void resetLaHoldProgress() {
  g_laHoldAccumMs = 0;
}

uint8_t unlockHoldPercent(uint32_t holdMs, bool uLockListening) {
  if (!uLockListening) {
    return 0;
  }
  if (config::kLaUnlockHoldMs == 0) {
    return 100;
  }
  if (holdMs >= config::kLaUnlockHoldMs) {
    return 100;
  }
  return static_cast<uint8_t>((holdMs * 100U) / config::kLaUnlockHoldMs);
}

PlayerUiPage currentPlayerUiPage() {
  return g_playerUi.page();
}

bool setPlayerUiPage(PlayerUiPage page) {
  g_playerUi.setPage(page);
  return true;
}

bool parsePlayerUiPageToken(const char* token, PlayerUiPage* outPage) {
  if (token == nullptr || outPage == nullptr) {
    return false;
  }
  if (strcmp(token, "NOW") == 0 || strcmp(token, "NOWPLAY") == 0 ||
      strcmp(token, "NOWPLAYING") == 0) {
    *outPage = PlayerUiPage::kNowPlaying;
    return true;
  }
  if (strcmp(token, "BROWSE") == 0 || strcmp(token, "BROWSER") == 0) {
    *outPage = PlayerUiPage::kBrowser;
    return true;
  }
  if (strcmp(token, "QUEUE") == 0) {
    *outPage = PlayerUiPage::kQueue;
    return true;
  }
  if (strcmp(token, "SET") == 0 || strcmp(token, "SETTINGS") == 0) {
    *outPage = PlayerUiPage::kSettings;
    return true;
  }
  return false;
}

const char* currentBrowsePath() {
  if (g_mp3BrowsePath.isEmpty()) {
    g_mp3BrowsePath = "/";
  }
  return g_mp3BrowsePath.c_str();
}

bool parseBackendModeToken(const char* token, PlayerBackendMode* outMode) {
  if (token == nullptr || outMode == nullptr) {
    return false;
  }
  if (strcmp(token, "AUTO") == 0 || strcmp(token, "AUTO_FALLBACK") == 0) {
    *outMode = PlayerBackendMode::kAutoFallback;
    return true;
  }
  if (strcmp(token, "AUDIO_TOOLS") == 0 || strcmp(token, "AUDIO_TOOLS_ONLY") == 0) {
    *outMode = PlayerBackendMode::kAudioToolsOnly;
    return true;
  }
  if (strcmp(token, "LEGACY") == 0 || strcmp(token, "LEGACY_ONLY") == 0) {
    *outMode = PlayerBackendMode::kLegacyOnly;
    return true;
  }
  return false;
}

uint8_t encodeBackendForScreen() {
  return static_cast<uint8_t>(g_mp3.activeBackend());
}

uint8_t encodeMp3ErrorForScreen() {
  const char* error = g_mp3.lastBackendError();
  if (error == nullptr || error[0] == '\0' || strcmp(error, "OK") == 0) {
    return 0U;
  }
  if (strcmp(error, "UNSUPPORTED") == 0) {
    return 1U;
  }
  if (strcmp(error, "OPEN_FAIL") == 0) {
    return 2U;
  }
  if (strcmp(error, "I2S_FAIL") == 0) {
    return 3U;
  }
  if (strcmp(error, "DEC_FAIL") == 0) {
    return 4U;
  }
  if (strcmp(error, "OOM") == 0) {
    return 5U;
  }
  if (strcmp(error, "RUNTIME") == 0) {
    return 6U;
  }
  return 99U;
}

void printMp3ScanStatus(const char* source) {
  const CatalogStats stats = g_mp3.catalogStats();
  Serial.printf("[MP3_SCAN] %s busy=%u tracks=%u folders=%u scan_ms=%lu indexed=%u metadata_best=%u\n",
                source,
                g_mp3.isScanBusy() ? 1U : 0U,
                static_cast<unsigned int>(stats.tracks),
                static_cast<unsigned int>(stats.folders),
                static_cast<unsigned long>(stats.scanMs),
                stats.indexed ? 1U : 0U,
                stats.metadataBestEffort ? 1U : 0U);
}

void printMp3BrowseList(const char* source, const char* path, uint16_t offset, uint16_t limit) {
  const char* safePath = (path == nullptr || path[0] == '\0') ? "/" : path;
  if (!g_mp3.isSdReady()) {
    Serial.printf("[MP3_BROWSE] %s OUT_OF_CONTEXT sd=0\n", source);
    return;
  }
  const uint16_t total = g_mp3.listTracks(safePath, offset, limit, Serial);
  Serial.printf("[MP3_BROWSE] %s path=%s total=%u offset=%u limit=%u\n",
                source,
                safePath,
                static_cast<unsigned int>(total),
                static_cast<unsigned int>(offset),
                static_cast<unsigned int>(limit));
}

void sendScreenFrameSnapshot(uint32_t nowMs, uint8_t keyForScreen) {
  const bool laDetected =
      (g_mode == RuntimeMode::kSignal) && g_laDetectionEnabled && g_laDetector.isDetected();
  const bool uLockMode = (g_mode == RuntimeMode::kSignal) && !g_uSonFunctional;
  const bool uLockListening = uLockMode && g_uLockListening;
  const bool uSonFunctional = (g_mode == RuntimeMode::kSignal) && g_uSonFunctional;
  const float micRms = g_laDetector.micRms();
  const uint8_t micLevelPercent = micLevelPercentFromRms(micRms);

  ScreenFrame frame;
  frame.laDetected = laDetected;
  frame.mp3Playing = g_mp3.isPlaying();
  frame.sdReady = g_mp3.isSdReady();
  frame.mp3Mode = (g_mode == RuntimeMode::kMp3);
  frame.uLockMode = uLockMode;
  frame.uLockListening = uLockListening;
  frame.uSonFunctional = uSonFunctional;
  frame.key = keyForScreen;
  frame.track = g_mp3.currentTrackNumber();
  frame.trackCount = g_mp3.trackCount();
  frame.volumePercent = g_mp3.volumePercent();
  frame.micLevelPercent = micLevelPercent;
  frame.tuningOffset = uLockListening ? g_laDetector.tuningOffset() : 0;
  frame.tuningConfidence = uLockListening ? g_laDetector.tuningConfidence() : 0;
  frame.micScopeEnabled = config::kScreenEnableMicScope && config::kUseI2SMicInput;
  frame.unlockHoldPercent = unlockHoldPercent(g_laHoldAccumMs, uLockListening);
  frame.startupStage = g_bootAudioProtocol.active ? 1U : 0U;
  frame.uiPage = static_cast<uint8_t>(currentPlayerUiPage());
  frame.repeatMode = static_cast<uint8_t>(g_mp3.repeatMode());
  frame.fxActive = g_mp3.isFxActive();
  frame.backendMode = encodeBackendForScreen();
  frame.scanBusy = g_mp3.isScanBusy();
  frame.errorCode = encodeMp3ErrorForScreen();

  if (frame.mp3Mode) {
    frame.appStage = 3U;
  } else if (!uSonFunctional) {
    frame.appStage = uLockListening ? 1U : 0U;
  } else {
    frame.appStage = 2U;
  }

  frame.sequence = ++g_screenFrameSeq;
  frame.nowMs = nowMs;
  g_screen.update(frame);
}

void stopUnlockJingle(bool restoreMicCapture) {
  if (!g_unlockJingle.active && !g_unlockJinglePlayer.isActive()) {
    return;
  }

  g_unlockJinglePlayer.stop();
  if (restoreMicCapture && g_unlockJingle.restoreMicCapture && g_mode == RuntimeMode::kSignal &&
      g_laDetectionEnabled) {
    g_laDetector.setCaptureEnabled(true);
  }

  g_unlockJingle.active = false;
  g_unlockJingle.restoreMicCapture = false;
}

void startUnlockJingle(uint32_t nowMs) {
  (void)nowMs;
  stopUnlockJingle(false);

  if (!config::kEnableUnlockI2sJingle) {
    return;
  }

  g_unlockJingle.restoreMicCapture = false;
  if (config::kUseI2SMicInput && g_laDetectionEnabled) {
    g_laDetector.setCaptureEnabled(false);
    g_unlockJingle.restoreMicCapture = true;
  }

  if (!g_unlockJinglePlayer.start(kUnlockJingleRtttl, config::kUnlockI2sJingleGain)) {
    if (g_unlockJingle.restoreMicCapture) {
      g_laDetector.setCaptureEnabled(true);
    }
    g_unlockJingle.active = false;
    g_unlockJingle.restoreMicCapture = false;
    Serial.println("[AUDIO] Unlock jingle I2S start failed.");
    return;
  }

  g_unlockJingle.active = true;
  Serial.println("[AUDIO] Unlock jingle I2S start.");
}

void updateUnlockJingle(uint32_t nowMs) {
  (void)nowMs;
  if (!g_unlockJingle.active) {
    return;
  }

  g_unlockJinglePlayer.update();
  if (g_unlockJinglePlayer.isActive()) {
    return;
  }

  if (g_unlockJingle.restoreMicCapture && g_mode == RuntimeMode::kSignal && g_laDetectionEnabled) {
    g_laDetector.setCaptureEnabled(true);
  }
  g_unlockJingle.active = false;
  g_unlockJingle.restoreMicCapture = false;
  Serial.println("[AUDIO] Unlock jingle I2S done.");
}

void playBootI2sNoiseFx() {
  if (!config::kEnableBootI2sNoiseFx || config::kBootI2sNoiseDurationMs == 0U) {
    return;
  }

  setBootAudioPaEnabled(true, "boot_noise_fx");
  printBootAudioOutputInfo("boot_noise_fx");

  const bool shouldRestoreMicCapture =
      config::kUseI2SMicInput && g_mode == RuntimeMode::kSignal && g_laDetectionEnabled;
  if (shouldRestoreMicCapture) {
    g_laDetector.setCaptureEnabled(false);
  }

  AudioOutputI2S output(static_cast<int>(config::kI2sOutputPort), AudioOutputI2S::EXTERNAL_I2S);
  output.SetPinout(static_cast<int>(config::kPinI2SBclk),
                   static_cast<int>(config::kPinI2SLrc),
                   static_cast<int>(config::kPinI2SDout));
  output.SetOutputModeMono(true);
  output.SetGain(config::kBootI2sNoiseGain);
  output.SetRate(static_cast<int>(config::kBootI2sNoiseSampleRateHz));
  output.SetBitsPerSample(16);
  output.SetChannels(2);
  if (!output.begin()) {
    if (shouldRestoreMicCapture) {
      g_laDetector.setCaptureEnabled(true);
    }
    Serial.println("[AUDIO] Boot noise I2S start failed.");
    return;
  }

  const uint32_t sampleRate = static_cast<uint32_t>(config::kBootI2sNoiseSampleRateHz);
  const uint32_t totalSamples =
      (sampleRate * static_cast<uint32_t>(config::kBootI2sNoiseDurationMs)) / 1000UL;
  const uint32_t attackSamples =
      (sampleRate * static_cast<uint32_t>(config::kBootI2sNoiseAttackMs)) / 1000UL;
  const uint32_t releaseSamples =
      (sampleRate * static_cast<uint32_t>(config::kBootI2sNoiseReleaseMs)) / 1000UL;
  const uint32_t sweepPeriodSamples = (sampleRate * 210UL) / 1000UL;

  constexpr float kTwoPi = 6.28318530718f;
  constexpr float kSweepStartHz = 180.0f;
  constexpr float kSweepEndHz = 3600.0f;
  constexpr float kToneBurstBaseHz = 980.0f;
  constexpr float kToneBurstSwingHz = 620.0f;
  constexpr float kToneBurstLfoHz = 6.5f;

  float sweepPhase = 0.0f;
  float tonePhase = 0.0f;
  float noiseHistory = 0.0f;
  float crackle = 0.0f;
  uint32_t sweepCycle = 0;
  uint32_t sweepPosInCycle = 0;
  bool interruptedByBootKey = false;
  bool stalled = false;
  uint32_t nextBootKeyPollSample = 0U;

  for (uint32_t i = 0; i < totalSamples; ++i) {
    if (i >= nextBootKeyPollSample) {
      nextBootKeyPollSample = i + 192U;
      if (serviceInputDuringBlockingAudio(millis(), "boot_noise_fx")) {
        interruptedByBootKey = true;
        break;
      }
    }

    uint32_t envPermille = 1000U;
    if (attackSamples > 0U && i < attackSamples) {
      envPermille = (i * 1000U) / attackSamples;
    }

    const uint32_t samplesLeft = totalSamples - i;
    if (releaseSamples > 0U && samplesLeft < releaseSamples) {
      const uint32_t releaseEnv = (samplesLeft * 1000U) / releaseSamples;
      if (releaseEnv < envPermille) {
        envPermille = releaseEnv;
      }
    }

    float sweepT =
        (sweepPeriodSamples > 0U)
            ? (static_cast<float>(sweepPosInCycle) / static_cast<float>(sweepPeriodSamples))
            : 0.0f;
    if ((sweepCycle & 1U) != 0U) {
      sweepT = 1.0f - sweepT;
    }
    const float sweepHz = kSweepStartHz + (kSweepEndHz - kSweepStartHz) * sweepT;
    sweepPhase += kTwoPi * (sweepHz / static_cast<float>(sampleRate));
    if (sweepPhase >= kTwoPi) {
      sweepPhase -= kTwoPi;
    }

    const float rawNoise = static_cast<float>(random(-32768L, 32767L)) / 32768.0f;
    const float hiss = rawNoise - (noiseHistory * 0.93f);
    noiseHistory = rawNoise;

    if (random(0L, 1000L) < 9L) {
      crackle = static_cast<float>(random(-32768L, 32767L)) / 16384.0f;
    }
    const float crackleSample = crackle;
    crackle *= 0.84f;

    const float toneLfo = sinf(kTwoPi * kToneBurstLfoHz * (static_cast<float>(i) / sampleRate));
    const float toneHz = kToneBurstBaseHz + (kToneBurstSwingHz * toneLfo);
    tonePhase += kTwoPi * (toneHz / static_cast<float>(sampleRate));
    if (tonePhase >= kTwoPi) {
      tonePhase -= kTwoPi;
    }

    const bool toneBurstOn = ((i / (sampleRate / 17U)) % 9U) < 2U;
    const bool dropout = ((i / (sampleRate / 26U)) % 13U) == 5U;
    const float am = 0.45f + 0.55f * sinf(kTwoPi * 11.0f * (static_cast<float>(i) / sampleRate));

    float sampleF = 0.0f;
    sampleF += 0.50f * sinf(sweepPhase);
    sampleF += 0.62f * hiss;
    sampleF += 0.28f * crackleSample;
    if (toneBurstOn) {
      sampleF += 0.30f * sinf(tonePhase);
    }
    sampleF *= am;
    if (dropout) {
      sampleF *= 0.14f;
    }
    sampleF *= static_cast<float>(envPermille) / 1000.0f;

    if (sampleF > 1.0f) {
      sampleF = 1.0f;
    } else if (sampleF < -1.0f) {
      sampleF = -1.0f;
    }

    const int16_t sample = static_cast<int16_t>(sampleF * 23000.0f);
    int16_t stereo[2] = {sample, sample};
    uint16_t waitGuard = 0U;
    while (!output.ConsumeSample(stereo)) {
      delayMicroseconds(40);
      ++waitGuard;
      if (waitGuard >= 1500U) {
        stalled = true;
        break;
      }
    }
    if (stalled) {
      break;
    }

    ++sweepPosInCycle;
    if (sweepPosInCycle >= sweepPeriodSamples && sweepPeriodSamples > 0U) {
      sweepPosInCycle = 0;
      ++sweepCycle;
    }
  }

  output.flush();
  output.stop();
  if (shouldRestoreMicCapture) {
    g_laDetector.setCaptureEnabled(true);
  }
  const char* outcome = stalled ? "stalled" : (interruptedByBootKey ? "interrupted" : "done");
  Serial.printf("[AUDIO] Boot noise I2S %s.\n", outcome);
}

void stopBootRadioScan(const char* source) {
  if (!g_bootRadioScanFx.isActive()) {
    return;
  }

  g_bootRadioScanFx.stop();

  if (g_bootRadioScan.restoreMicCapture && config::kUseI2SMicInput && g_mode == RuntimeMode::kSignal &&
      g_laDetectionEnabled) {
    g_laDetector.setCaptureEnabled(true);
  }

  g_bootRadioScan.restoreMicCapture = false;
  g_bootRadioScan.lastLogMs = 0;
  Serial.printf("[AUDIO] %s radio scan stop.\n", source);
}

bool startBootRadioScan(const char* source) {
  stopBootRadioScan("boot_radio_restart");

  const uint32_t sampleRate =
      (config::kBootI2sNoiseSampleRateHz > 0U) ? static_cast<uint32_t>(config::kBootI2sNoiseSampleRateHz)
                                               : 22050U;

  g_bootRadioScan.restoreMicCapture =
      config::kUseI2SMicInput && g_mode == RuntimeMode::kSignal && g_laDetectionEnabled;
  if (g_bootRadioScan.restoreMicCapture) {
    g_laDetector.setCaptureEnabled(false);
  }

  setBootAudioPaEnabled(true, source);
  printBootAudioOutputInfo(source);

  g_bootRadioScanFx.setGain(config::kBootI2sNoiseGain);
  g_bootRadioScanFx.setSampleRate(sampleRate);
  if (!g_bootRadioScanFx.start(FmRadioScanFx::Effect::kFmSweep)) {
    if (g_bootRadioScan.restoreMicCapture) {
      g_laDetector.setCaptureEnabled(true);
    }
    g_bootRadioScan.restoreMicCapture = false;
    Serial.printf("[AUDIO] %s radio scan start failed.\n", source);
    return false;
  }

  g_bootRadioScan.lastLogMs = millis();

  Serial.printf("[AUDIO] %s radio scan start (Mozzi+AudioTools) sr=%luHz chunk=%ums\n",
                source,
                static_cast<unsigned long>(sampleRate),
                static_cast<unsigned int>(config::kBootRadioScanChunkMs));
  return true;
}

void updateBootRadioScan(uint32_t nowMs) {
  if (!g_bootRadioScanFx.isActive()) {
    return;
  }

  g_bootRadioScanFx.update(nowMs, config::kBootRadioScanChunkMs);

  if (static_cast<int32_t>(nowMs - g_bootRadioScan.lastLogMs) >= 0) {
    Serial.println("[AUDIO] radio scan active (attente touche).");
    g_bootRadioScan.lastLogMs = nowMs + 4000U;
  }
}

void setBootAudioPaEnabled(bool enabled, const char* source) {
  if (config::kPinAudioPaEnable < 0) {
    return;
  }
  g_paEnabledRequest = enabled;
  const bool outputHigh = g_paEnableActiveHigh ? enabled : !enabled;
  pinMode(static_cast<uint8_t>(config::kPinAudioPaEnable), OUTPUT);
  digitalWrite(static_cast<uint8_t>(config::kPinAudioPaEnable), outputHigh ? HIGH : LOW);
  Serial.printf("[AUDIO_DBG] %s PA_REQ=%s pin=%d level=%s pol=%s\n",
                source,
                enabled ? "ON" : "OFF",
                static_cast<int>(config::kPinAudioPaEnable),
                outputHigh ? "HIGH" : "LOW",
                g_paEnableActiveHigh ? "ACTIVE_HIGH" : "ACTIVE_LOW");
}

void printBootAudioOutputInfo(const char* source) {
  int paRawState = -1;
  int paEnabledState = -1;
  if (config::kPinAudioPaEnable >= 0) {
    paRawState = digitalRead(static_cast<uint8_t>(config::kPinAudioPaEnable));
    const bool rawHigh = paRawState != LOW;
    const bool paEnabled = g_paEnableActiveHigh ? rawHigh : !rawHigh;
    paEnabledState = paEnabled ? 1 : 0;
  }

  Serial.printf(
      "[AUDIO_DBG] %s i2s_port=%u bclk=%u lrc=%u dout=%u sr=%u boot_gain=%.2f pa_raw=%d pa_en=%d pa_pol=%s\n",
      source,
      static_cast<unsigned int>(config::kI2sOutputPort),
      static_cast<unsigned int>(config::kPinI2SBclk),
      static_cast<unsigned int>(config::kPinI2SLrc),
      static_cast<unsigned int>(config::kPinI2SDout),
      static_cast<unsigned int>(config::kBootI2sNoiseSampleRateHz),
      static_cast<double>(config::kBootI2sNoiseGain),
      paRawState,
      paEnabledState,
      g_paEnableActiveHigh ? "ACTIVE_HIGH" : "ACTIVE_LOW");
}

enum class BootFsCodec : uint8_t {
  kUnknown = 0,
  kMp3,
  kWav,
  kAac,
  kFlac,
  kOpus,
};

BootFsCodec bootFsCodecFromPath(const char* path) {
  if (path == nullptr || path[0] == '\0') {
    return BootFsCodec::kUnknown;
  }
  String lower(path);
  lower.toLowerCase();
  if (lower.endsWith(".mp3")) {
    return BootFsCodec::kMp3;
  }
  if (lower.endsWith(".wav")) {
    return BootFsCodec::kWav;
  }
  if (lower.endsWith(".aac")) {
    return BootFsCodec::kAac;
  }
  if (lower.endsWith(".flac")) {
    return BootFsCodec::kFlac;
  }
  if (lower.endsWith(".opus") || lower.endsWith(".ogg")) {
    return BootFsCodec::kOpus;
  }
  return BootFsCodec::kUnknown;
}

const char* bootFsCodecLabel(BootFsCodec codec) {
  switch (codec) {
    case BootFsCodec::kMp3:
      return "MP3";
    case BootFsCodec::kWav:
      return "WAV";
    case BootFsCodec::kAac:
      return "AAC";
    case BootFsCodec::kFlac:
      return "FLAC";
    case BootFsCodec::kOpus:
      return "OPUS";
    case BootFsCodec::kUnknown:
    default:
      return "UNKNOWN";
  }
}

AudioGenerator* createBootFsDecoder(BootFsCodec codec) {
  switch (codec) {
    case BootFsCodec::kMp3:
      return new (std::nothrow) AudioGeneratorMP3();
    case BootFsCodec::kWav:
      return new (std::nothrow) AudioGeneratorWAV();
    case BootFsCodec::kAac:
      return new (std::nothrow) AudioGeneratorAAC();
    case BootFsCodec::kFlac:
      return new (std::nothrow) AudioGeneratorFLAC();
    case BootFsCodec::kOpus:
      return new (std::nothrow) AudioGeneratorOpus();
    case BootFsCodec::kUnknown:
    default:
      return nullptr;
  }
}

bool isSupportedBootFsAudioPath(const char* path) {
  return bootFsCodecFromPath(path) != BootFsCodec::kUnknown;
}

bool resolveBootLittleFsPath(String* outPath) {
  if (outPath == nullptr || !g_littleFsReady) {
    return false;
  }
  outPath->remove(0);

  static const char* kBootCandidates[] = {
      "/boot.mp3", "/boot.wav", "/boot.aac", "/boot.flac", "/boot.opus", "/boot.ogg"};
  int8_t bestCandidateIndex = -1;
  String bestCandidatePath;
  String preferredPath;
  const bool hasPreferred = config::kBootFxLittleFsPath[0] != '\0' &&
                            isSupportedBootFsAudioPath(config::kBootFxLittleFsPath);
  if (hasPreferred) {
    preferredPath = String(config::kBootFxLittleFsPath);
    preferredPath.toLowerCase();
  }
  String firstSupported;
  fs::File root = LittleFS.open("/");
  if (root && root.isDirectory()) {
    fs::File file = root.openNextFile();
    while (file) {
      if (!file.isDirectory()) {
        String name = String(file.name());
        if (!name.startsWith("/")) {
          name = "/" + name;
        }
        if (isSupportedBootFsAudioPath(name.c_str())) {
          if (firstSupported.isEmpty()) {
            firstSupported = name;
          }
          String lowerName = name;
          lowerName.toLowerCase();
          if (hasPreferred && lowerName == preferredPath) {
            *outPath = name;
            file.close();
            root.close();
            return true;
          }
          for (uint8_t i = 0; i < (sizeof(kBootCandidates) / sizeof(kBootCandidates[0])); ++i) {
            if (lowerName == kBootCandidates[i]) {
              if (bestCandidateIndex < 0 || static_cast<int8_t>(i) < bestCandidateIndex) {
                bestCandidateIndex = static_cast<int8_t>(i);
                bestCandidatePath = name;
              }
              break;
            }
          }
        }
      }
      file.close();
      file = root.openNextFile();
    }
    root.close();
  }

  if (bestCandidateIndex >= 0) {
    *outPath = bestCandidatePath;
    return true;
  }
  if (!firstSupported.isEmpty()) {
    *outPath = firstSupported;
    return true;
  }
  return false;
}

void setupInternalLittleFs() {
  g_littleFsReady = false;
  if (!config::kEnableInternalLittleFs) {
    Serial.println("[FS] LittleFS disabled by config.");
    return;
  }

  g_littleFsReady = LittleFS.begin(config::kInternalLittleFsFormatOnFail);
  if (!g_littleFsReady) {
    Serial.printf("[FS] LittleFS mount failed (format_on_fail=%u).\n",
                  config::kInternalLittleFsFormatOnFail ? 1U : 0U);
    Serial.println("[FS] Upload assets with: pio run -e esp32dev -t uploadfs");
    return;
  }

  printLittleFsInfo("boot");
  String bootFxPath;
  if (!resolveBootLittleFsPath(&bootFxPath)) {
    Serial.printf("[FS] Boot FX absent (path prefere: %s, fallback noise active).\n",
                  config::kBootFxLittleFsPath);
  } else {
    Serial.printf("[FS] Boot FX ready: %s\n", bootFxPath.c_str());
  }
}

void printLittleFsInfo(const char* source) {
  if (!config::kEnableInternalLittleFs) {
    Serial.printf("[FS] %s LittleFS disabled by config.\n", source);
    return;
  }
  if (!g_littleFsReady) {
    Serial.printf("[FS] %s LittleFS not mounted.\n", source);
    return;
  }
  const size_t used = LittleFS.usedBytes();
  const size_t total = LittleFS.totalBytes();
  Serial.printf("[FS] %s LittleFS mounted used=%u/%u bytes free=%u\n",
                source,
                static_cast<unsigned int>(used),
                static_cast<unsigned int>(total),
                static_cast<unsigned int>((total > used) ? (total - used) : 0U));
}

void listLittleFsRoot(const char* source) {
  if (!g_littleFsReady) {
    Serial.printf("[FS] %s list refused: LittleFS not mounted.\n", source);
    return;
  }

  fs::File root = LittleFS.open("/");
  if (!root || !root.isDirectory()) {
    Serial.printf("[FS] %s cannot open root '/'.\n", source);
    return;
  }

  Serial.printf("[FS] %s list '/':\n", source);
  uint16_t count = 0;
  fs::File file = root.openNextFile();
  while (file) {
    Serial.printf("[FS]   %s %s size=%u\n",
                  file.isDirectory() ? "DIR " : "FILE",
                  file.name(),
                  static_cast<unsigned int>(file.size()));
    ++count;
    file.close();
    file = root.openNextFile();
  }
  root.close();
  Serial.printf("[FS] %s list done (%u entry).\n",
                source,
                static_cast<unsigned int>(count));
}

bool serviceInputDuringBlockingAudio(uint32_t nowMs, const char* source) {
  static uint32_t nextScreenKeepAliveMs = 0U;
  if (static_cast<int32_t>(nowMs - nextScreenKeepAliveMs) >= 0) {
    sendScreenFrameSnapshot(nowMs, 0U);
    nextScreenKeepAliveMs = nowMs + 120U;
  }

  g_keypad.update(nowMs);

  static bool dispatchGuard = false;
  if (dispatchGuard) {
    return false;
  }

  uint8_t pressedKey = 0;
  uint16_t pressedRaw = 0;
  if (!g_keypad.consumePress(&pressedKey, &pressedRaw)) {
    return false;
  }

  Serial.printf("[KEY] K%u raw=%u (%s)\n",
                static_cast<unsigned int>(pressedKey),
                static_cast<unsigned int>(pressedRaw),
                source);
  dispatchGuard = true;
  if (g_bootAudioProtocol.active) {
    handleBootAudioProtocolKey(pressedKey, nowMs);
  } else if (g_keySelfTest.active) {
    handleKeySelfTestPress(pressedKey, pressedRaw);
  } else {
    handleKeyPress(pressedKey);
  }
  dispatchGuard = false;
  sendScreenFrameSnapshot(nowMs, pressedKey);
  nextScreenKeepAliveMs = nowMs + 120U;
  return true;
}

bool playAudioFromFsBlocking(fs::FS& storage,
                             const char* path,
                             float gain,
                             uint32_t maxDurationMs,
                             const char* source) {
  if (path == nullptr || path[0] == '\0') {
    Serial.printf("[AUDIO_FS] %s invalid path.\n", source);
    return false;
  }
  if (!storage.exists(path)) {
    Serial.printf("[AUDIO_FS] %s missing file: %s\n", source, path);
    return false;
  }
  const BootFsCodec codec = bootFsCodecFromPath(path);
  if (codec == BootFsCodec::kUnknown) {
    Serial.printf("[AUDIO_FS] %s unsupported extension: %s\n", source, path);
    return false;
  }

  const bool shouldRestoreMicCapture =
      config::kUseI2SMicInput && g_mode == RuntimeMode::kSignal && g_laDetectionEnabled;
  if (shouldRestoreMicCapture) {
    g_laDetector.setCaptureEnabled(false);
  }

  setBootAudioPaEnabled(true, source);
  printBootAudioOutputInfo(source);

  AudioFileSourceFS file(storage, path);
  AudioOutputI2S output(static_cast<int>(config::kI2sOutputPort), AudioOutputI2S::EXTERNAL_I2S);
  output.SetPinout(static_cast<int>(config::kPinI2SBclk),
                   static_cast<int>(config::kPinI2SLrc),
                   static_cast<int>(config::kPinI2SDout));
  output.SetGain(gain);
  AudioGenerator* decoder = createBootFsDecoder(codec);
  if (decoder == nullptr) {
    if (shouldRestoreMicCapture) {
      g_laDetector.setCaptureEnabled(true);
    }
    Serial.printf("[AUDIO_FS] %s decoder alloc failed codec=%s\n",
                  source,
                  bootFsCodecLabel(codec));
    return false;
  }

  if (!decoder->begin(&file, &output)) {
    delete decoder;
    if (shouldRestoreMicCapture) {
      g_laDetector.setCaptureEnabled(true);
    }
    Serial.printf("[AUDIO_FS] %s decoder start failed [%s]: %s\n",
                  source,
                  bootFsCodecLabel(codec),
                  path);
    return false;
  }

  const uint32_t startMs = millis();
  bool timeout = false;
  bool interruptedByBootKey = false;
  while (decoder->isRunning()) {
    if (!decoder->loop()) {
      break;
    }
    const uint32_t nowMs = millis();
    if (maxDurationMs > 0U && static_cast<uint32_t>(nowMs - startMs) >= maxDurationMs) {
      timeout = true;
      break;
    }
    if (serviceInputDuringBlockingAudio(nowMs, source)) {
      interruptedByBootKey = true;
      break;
    }
    delay(0);
  }
  decoder->stop();
  delete decoder;
  output.stop();

  if (shouldRestoreMicCapture) {
    g_laDetector.setCaptureEnabled(true);
  }

  const char* status = timeout ? "timeout" : (interruptedByBootKey ? "interrupted" : "done");
  Serial.printf("[AUDIO_FS] %s %s [%s]: %s\n",
                source,
                status,
                bootFsCodecLabel(codec),
                path);
  return !timeout && !interruptedByBootKey;
}

bool playBootLittleFsFx(const char* source) {
  if (!config::kEnableInternalLittleFs || !g_littleFsReady) {
    return false;
  }
  String path;
  if (!resolveBootLittleFsPath(&path)) {
    Serial.printf("[AUDIO_FS] %s no supported boot FX in LittleFS (preferred=%s)\n",
                  source,
                  config::kBootFxLittleFsPath);
    return false;
  }

  Serial.printf("[AUDIO_FS] %s playing LittleFS boot FX: %s\n",
                source,
                path.c_str());
  return playAudioFromFsBlocking(LittleFS,
                                 path.c_str(),
                                 config::kBootFxLittleFsGain,
                                 config::kBootFxLittleFsMaxDurationMs,
                                 source);
}

bool resolveRandomFsPathContaining(fs::FS& storage, const char* token, String* outPath) {
  if (outPath == nullptr || token == nullptr || token[0] == '\0') {
    return false;
  }
  outPath->remove(0);

  String needle(token);
  needle.toLowerCase();
  if (needle.isEmpty()) {
    return false;
  }

  fs::File root = storage.open("/");
  if (!root || !root.isDirectory()) {
    return false;
  }

  uint32_t matches = 0;
  fs::File file = root.openNextFile();
  while (file) {
    if (!file.isDirectory()) {
      String name = String(file.name());
      if (!name.startsWith("/")) {
        name = "/" + name;
      }
      if (isSupportedBootFsAudioPath(name.c_str())) {
        String lowerName = name;
        lowerName.toLowerCase();
        if (lowerName.indexOf(needle) >= 0) {
          ++matches;
          if (matches == 1U || random(0L, static_cast<long>(matches)) == 0L) {
            *outPath = name;
          }
        }
      }
    }
    file.close();
    file = root.openNextFile();
  }
  root.close();
  return !outPath->isEmpty();
}

bool playRandomLittleFsTokenFx(const char* token,
                               const char* source,
                               float gain,
                               uint32_t maxDurationMs,
                               String* outPath) {
  if (!g_littleFsReady) {
    return false;
  }

  String path;
  if (!resolveRandomFsPathContaining(LittleFS, token, &path)) {
    return false;
  }

  if (outPath != nullptr) {
    *outPath = path;
  }
  Serial.printf("[AUDIO_FS] %s random '%s' from LittleFS: %s\n",
                source,
                token,
                path.c_str());
  return playAudioFromFsBlocking(LittleFS, path.c_str(), gain, maxDurationMs, source);
}

bool playRandomTokenFx(const char* token,
                       const char* source,
                       bool allowSdFallback,
                       uint32_t maxDurationMs) {
  String path;
  if (playRandomLittleFsTokenFx(token,
                                source,
                                config::kBootFxLittleFsGain,
                                maxDurationMs,
                                &path)) {
    return true;
  }

  if (!allowSdFallback) {
    return false;
  }

  if (!g_mp3.isSdReady()) {
    g_mp3.requestStorageRefresh();
    g_mp3.update(millis(), false);
  }
  if (!g_mp3.isSdReady()) {
    return false;
  }

  if (!resolveRandomFsPathContaining(SD_MMC, token, &path)) {
    return false;
  }
  Serial.printf("[AUDIO_FS] %s random '%s' from SD: %s\n",
                source,
                token,
                path.c_str());
  return playAudioFromFsBlocking(SD_MMC,
                                 path.c_str(),
                                 config::kBootFxLittleFsGain,
                                 maxDurationMs,
                                 source);
}

bool startAudioFromFsAsync(fs::FS& storage,
                           const char* path,
                           float gain,
                           uint32_t maxDurationMs,
                           const char* source) {
  if (path == nullptr || path[0] == '\0') {
    return false;
  }
  if (!storage.exists(path)) {
    Serial.printf("[AUDIO_ASYNC] %s missing file: %s\n", source, path);
    return false;
  }

  setBootAudioPaEnabled(true, source);
  printBootAudioOutputInfo(source);
  if (!audioService().startBaseFs(storage, path, gain, maxDurationMs, source)) {
    Serial.printf("[AUDIO_ASYNC] %s start failed: %s\n", source, path);
    return false;
  }
  Serial.printf("[AUDIO_ASYNC] %s start fs: %s\n", source, path);
  return true;
}

bool startBootAudioPrimaryFxAsync(const char* source) {
  if (config::kPreferLittleFsBootFx && g_littleFsReady) {
    String path;
    if (resolveBootLittleFsPath(&path)) {
      if (startAudioFromFsAsync(LittleFS,
                                path.c_str(),
                                config::kBootFxLittleFsGain,
                                config::kBootFxLittleFsMaxDurationMs,
                                source)) {
        return true;
      }
    }
  }

  if (!config::kEnableBootI2sNoiseFx) {
    return false;
  }

  setBootAudioPaEnabled(true, source);
  printBootAudioOutputInfo(source);
  const uint32_t durationMs =
      (config::kBootI2sNoiseDurationMs > 0U) ? static_cast<uint32_t>(config::kBootI2sNoiseDurationMs)
                                             : 1100U;
  const bool ok =
      audioService().startBaseFx(AudioEffectId::kFmSweep, config::kBootI2sNoiseGain, durationMs, source);
  if (ok) {
    Serial.printf("[AUDIO_ASYNC] %s fallback effect=%s dur=%lu ms\n",
                  source,
                  effectLabel(FmRadioScanFx::Effect::kFmSweep),
                  static_cast<unsigned long>(durationMs));
  }
  return ok;
}

bool startRandomTokenFxAsync(const char* token,
                             const char* source,
                             bool allowSdFallback,
                             uint32_t maxDurationMs) {
  if (token == nullptr || token[0] == '\0') {
    return false;
  }

  String path;
  if (g_littleFsReady && resolveRandomFsPathContaining(LittleFS, token, &path)) {
    Serial.printf("[AUDIO_ASYNC] %s random '%s' from LittleFS: %s\n",
                  source,
                  token,
                  path.c_str());
    return startAudioFromFsAsync(LittleFS,
                                 path.c_str(),
                                 config::kBootFxLittleFsGain,
                                 maxDurationMs,
                                 source);
  }

  if (!allowSdFallback) {
    return false;
  }

  if (!g_mp3.isSdReady()) {
    g_mp3.requestStorageRefresh();
    g_mp3.update(millis(), false);
  }
  if (!g_mp3.isSdReady()) {
    return false;
  }

  if (!resolveRandomFsPathContaining(SD_MMC, token, &path)) {
    return false;
  }
  Serial.printf("[AUDIO_ASYNC] %s random '%s' from SD: %s\n",
                source,
                token,
                path.c_str());
  return startAudioFromFsAsync(SD_MMC,
                               path.c_str(),
                               config::kBootFxLittleFsGain,
                               maxDurationMs,
                               source);
}

void updateAsyncAudioService(uint32_t nowMs) {
  audioService().update(nowMs);
}

void playRtttlJingleBlocking(const char* song, float gain, const char* source) {
  if (song == nullptr || song[0] == '\0') {
    return;
  }

  const bool shouldRestoreMicCapture =
      config::kUseI2SMicInput && g_mode == RuntimeMode::kSignal && g_laDetectionEnabled;
  if (shouldRestoreMicCapture) {
    g_laDetector.setCaptureEnabled(false);
  }
  setBootAudioPaEnabled(true, source);
  printBootAudioOutputInfo(source);

  if (!g_unlockJinglePlayer.start(song, gain)) {
    if (shouldRestoreMicCapture) {
      g_laDetector.setCaptureEnabled(true);
    }
    Serial.printf("[AUDIO] %s RTTTL start failed.\n", source);
    return;
  }

  const uint32_t timeoutMs = millis() + 8000U;
  bool interruptedByInput = false;
  while (g_unlockJinglePlayer.isActive()) {
    g_unlockJinglePlayer.update();
    const uint32_t nowMs = millis();
    if (static_cast<int32_t>(nowMs - timeoutMs) >= 0) {
      Serial.printf("[AUDIO] %s RTTTL timeout.\n", source);
      break;
    }
    if (serviceInputDuringBlockingAudio(nowMs, source)) {
      interruptedByInput = true;
      break;
    }
    delay(0);
  }
  g_unlockJinglePlayer.stop();

  if (shouldRestoreMicCapture) {
    g_laDetector.setCaptureEnabled(true);
  }
  if (interruptedByInput) {
    Serial.printf("[AUDIO] %s RTTTL interrupted.\n", source);
  }
}

const char* effectLabel(FmRadioScanFx::Effect effect) {
  return audioEffectLabel(effect);
}

bool playGeneratedI2sFxBlocking(FmRadioScanFx::Effect effect,
                                uint32_t durationMs,
                                float gain,
                                const char* source) {
  if (durationMs == 0U) {
    return true;
  }

  const bool shouldRestoreMicCapture =
      config::kUseI2SMicInput && g_mode == RuntimeMode::kSignal && g_laDetectionEnabled;
  if (shouldRestoreMicCapture) {
    g_laDetector.setCaptureEnabled(false);
  }

  setBootAudioPaEnabled(true, source);
  printBootAudioOutputInfo(source);
  g_bootRadioScanFx.setGain(gain);
  g_bootRadioScanFx.setSampleRate(
      (config::kBootI2sNoiseSampleRateHz > 0U) ? static_cast<uint32_t>(config::kBootI2sNoiseSampleRateHz)
                                               : 22050U);
  bool ok = g_bootRadioScanFx.start(effect);
  bool interruptedByInput = false;
  if (ok) {
    const uint32_t deadlineMs = millis() + durationMs;
    while (g_bootRadioScanFx.isActive()) {
      const uint32_t nowMs = millis();
      if (static_cast<int32_t>(nowMs - deadlineMs) >= 0) {
        break;
      }
      g_bootRadioScanFx.update(nowMs, config::kBootRadioScanChunkMs);
      if (serviceInputDuringBlockingAudio(nowMs, source)) {
        interruptedByInput = true;
        break;
      }
      delay(0);
    }
    g_bootRadioScanFx.stop();
  }

  if (shouldRestoreMicCapture) {
    g_laDetector.setCaptureEnabled(true);
  }

  Serial.printf("[AUDIO_FX] %s effect=%s %s dur=%lums\n",
                source,
                effectLabel(effect),
                ok ? (interruptedByInput ? "interrupted" : "done") : "failed",
                static_cast<unsigned long>(durationMs));
  return ok && !interruptedByInput;
}

void resetStoryTimeline(const char* source) {
  storyController().reset(source);
}

void armStoryTimelineAfterUnlock(uint32_t nowMs) {
  storyController().onUnlock(nowMs, "unlock");
}

bool isMp3GateOpen() {
  return storyController().isMp3GateOpen();
}

void updateStoryTimeline(uint32_t nowMs) {
  storyController().update(nowMs);
}

void playBootAudioPrimaryFx(const char* source) {
  if (config::kPreferLittleFsBootFx) {
    if (playBootLittleFsFx(source)) {
      return;
    }
  }
  if (config::kEnableBootI2sNoiseFx) {
    playBootI2sNoiseFx();
    return;
  }
  Serial.printf("[AUDIO] %s no boot FX source configured.\n", source);
}

void extendBootAudioProtocolWindow(uint32_t nowMs) {
  if (!g_bootAudioProtocol.active) {
    return;
  }
  g_bootAudioProtocol.nextReminderMs = nowMs + static_cast<uint32_t>(config::kBootProtocolPromptPeriodMs);
}

void playBootI2sToneFx(float freqHz, uint16_t durationMs, float gain, const char* source) {
  if (durationMs == 0U || freqHz <= 0.0f) {
    return;
  }

  const bool shouldRestoreMicCapture =
      config::kUseI2SMicInput && g_mode == RuntimeMode::kSignal && g_laDetectionEnabled;
  if (shouldRestoreMicCapture) {
    g_laDetector.setCaptureEnabled(false);
  }

  setBootAudioPaEnabled(true, source);
  printBootAudioOutputInfo(source);

  AudioOutputI2S output(static_cast<int>(config::kI2sOutputPort), AudioOutputI2S::EXTERNAL_I2S);
  output.SetPinout(static_cast<int>(config::kPinI2SBclk),
                   static_cast<int>(config::kPinI2SLrc),
                   static_cast<int>(config::kPinI2SDout));
  output.SetOutputModeMono(true);
  output.SetGain(gain);
  output.SetRate(static_cast<int>(config::kBootI2sNoiseSampleRateHz));
  output.SetBitsPerSample(16);
  output.SetChannels(2);
  if (!output.begin()) {
    if (shouldRestoreMicCapture) {
      g_laDetector.setCaptureEnabled(true);
    }
    Serial.printf("[AUDIO_DBG] %s tone start failed.\n", source);
    return;
  }

  constexpr float kTwoPi = 6.28318530718f;
  const uint32_t sampleRate = static_cast<uint32_t>(config::kBootI2sNoiseSampleRateHz);
  const uint32_t totalSamples = (sampleRate * static_cast<uint32_t>(durationMs)) / 1000UL;
  const uint32_t attackSamplesCalc = (sampleRate * 20UL) / 1000UL;
  const uint32_t releaseSamplesCalc = (sampleRate * 40UL) / 1000UL;
  const uint32_t attackSamples = (attackSamplesCalc > 0U) ? attackSamplesCalc : 1U;
  const uint32_t releaseSamples = (releaseSamplesCalc > 0U) ? releaseSamplesCalc : 1U;
  const float phaseInc = kTwoPi * (freqHz / static_cast<float>(sampleRate));
  float phase = 0.0f;
  bool interruptedByInput = false;
  bool stalled = false;
  uint32_t nextInputPollSample = 0U;

  for (uint32_t i = 0; i < totalSamples; ++i) {
    if (i >= nextInputPollSample) {
      nextInputPollSample = i + 192U;
      if (serviceInputDuringBlockingAudio(millis(), source)) {
        interruptedByInput = true;
        break;
      }
    }

    float env = 1.0f;
    if (i < attackSamples) {
      env = static_cast<float>(i) / static_cast<float>(attackSamples);
    }
    const uint32_t left = totalSamples - i;
    if (left < releaseSamples) {
      const float releaseEnv = static_cast<float>(left) / static_cast<float>(releaseSamples);
      if (releaseEnv < env) {
        env = releaseEnv;
      }
    }

    const float sampleF = sinf(phase) * env;
    const int16_t sample = static_cast<int16_t>(sampleF * 24000.0f);
    int16_t stereo[2] = {sample, sample};
    uint16_t waitGuard = 0U;
    while (!output.ConsumeSample(stereo)) {
      delayMicroseconds(40);
      ++waitGuard;
      if (waitGuard >= 1500U) {
        stalled = true;
        break;
      }
    }
    if (stalled) {
      break;
    }

    phase += phaseInc;
    if (phase >= kTwoPi) {
      phase -= kTwoPi;
    }
  }

  output.flush();
  output.stop();
  if (shouldRestoreMicCapture) {
    g_laDetector.setCaptureEnabled(true);
  }

  const char* outcome = stalled ? "stalled" : (interruptedByInput ? "interrupted" : "done");
  Serial.printf("[AUDIO_DBG] %s tone %s freq=%.1fHz gain=%.2f dur=%ums\n",
                source,
                outcome,
                static_cast<double>(freqHz),
                static_cast<double>(gain),
                static_cast<unsigned int>(durationMs));
}

void cooperativeDelayWithInput(uint16_t delayMs, const char* source) {
  const uint32_t deadlineMs = millis() + static_cast<uint32_t>(delayMs);
  while (static_cast<int32_t>(millis() - deadlineMs) < 0) {
    if (serviceInputDuringBlockingAudio(millis(), source)) {
      return;
    }
    delay(0);
  }
}

void playBootAudioDiagSequence() {
  Serial.println("[AUDIO_DBG] Diag sequence: 220Hz -> 440Hz -> 880Hz");
  playBootI2sToneFx(220.0f, 260U, 0.28f, "diag_220");
  cooperativeDelayWithInput(70U, "diag_wait_1");
  playBootI2sToneFx(440.0f, 260U, 0.46f, "diag_440");
  cooperativeDelayWithInput(70U, "diag_wait_2");
  playBootI2sToneFx(880.0f, 260U, 0.64f, "diag_880");
}

uint32_t randomBootLoopScanDurationMs() {
  const long minMs = static_cast<long>(kBootLoopScanMinMs);
  const long maxMsExclusive = static_cast<long>(kBootLoopScanMaxMs + 1U);
  return static_cast<uint32_t>(random(minMs, maxMsExclusive));
}

void armBootAudioLoopScanWindow(uint32_t nowMs, const char* source) {
  const uint32_t scanDurationMs = randomBootLoopScanDurationMs();
  g_bootAudioProtocol.deadlineMs = nowMs + scanDurationMs;
  Serial.printf("[BOOT_PROTO] %s scan window=%lu ms (10..40s)\n",
                source,
                static_cast<unsigned long>(scanDurationMs));
}

void startBootAudioLoopCycle(uint32_t nowMs, const char* source) {
  if (!g_bootAudioProtocol.active) {
    return;
  }

  ++g_bootAudioProtocol.replayCount;
  Serial.printf("[BOOT_PROTO] LOOP #%u via=%s\n",
                static_cast<unsigned int>(g_bootAudioProtocol.replayCount),
                source);

  g_bootAudioProtocol.waitingAudio = false;
  g_bootAudioProtocol.cycleSourceTag[0] = '\0';
  if (source != nullptr && source[0] != '\0') {
    snprintf(g_bootAudioProtocol.cycleSourceTag,
             sizeof(g_bootAudioProtocol.cycleSourceTag),
             "%s",
             source);
  }

  stopBootRadioScan("boot_proto_cycle");
  audioService().stopBase("boot_proto_cycle");

  bool startedAudio = startRandomTokenFxAsync("BOOT", source, false);
  if (!startedAudio) {
    Serial.println("[BOOT_PROTO] Aucun fichier contenant 'BOOT': fallback FX standard.");
    startedAudio = startBootAudioPrimaryFxAsync(source);
  }
  if (!g_bootAudioProtocol.active) {
    Serial.printf("[BOOT_PROTO] LOOP aborted after key action (%s)\n", source);
    return;
  }

  if (startedAudio) {
    g_bootAudioProtocol.waitingAudio = true;
    g_bootAudioProtocol.deadlineMs = 0U;
    g_bootAudioProtocol.nextReminderMs =
        nowMs + static_cast<uint32_t>(config::kBootProtocolPromptPeriodMs);
    return;
  }

  if (!startBootRadioScan(source)) {
    g_bootAudioProtocol.deadlineMs = millis() + 5000U;
    Serial.println("[BOOT_PROTO] Radio scan KO, retry auto dans 5s.");
    return;
  }

  const uint32_t afterAudioNowMs = millis();
  armBootAudioLoopScanWindow(afterAudioNowMs, source);
  g_bootAudioProtocol.nextReminderMs =
      afterAudioNowMs + static_cast<uint32_t>(config::kBootProtocolPromptPeriodMs);
}

void printBootAudioProtocolHelp() {
  Serial.println("[BOOT_PROTO] Boucle auto: random '*boot*' + scan radio I2S (10..40s), puis repeat.");
  Serial.println("[BOOT_PROTO] Touches: K1..K6 = NEXT (lance U_LOCK ecoute)");
  Serial.println(
      "[BOOT_PROTO] Serial: BOOT_NEXT | BOOT_REPLAY | BOOT_STATUS | BOOT_HELP | BOOT_REOPEN");
  Serial.println(
      "[BOOT_PROTO] Serial: BOOT_TEST_TONE | BOOT_TEST_DIAG | BOOT_PA_ON | BOOT_PA_OFF | BOOT_PA_STATUS | BOOT_PA_INV");
  Serial.println("[BOOT_PROTO] Serial: BOOT_FS_INFO | BOOT_FS_LIST | BOOT_FS_TEST");
  Serial.println("[BOOT_PROTO] Serial FX: BOOT_FX_FM | BOOT_FX_SONAR | BOOT_FX_MORSE | BOOT_FX_WIN");
  Serial.println("[BOOT_PROTO] Codec debug: CODEC_STATUS | CODEC_DUMP | CODEC_RD/WR | CODEC_VOL");
}

const char* runtimeModeLabel() {
  if (g_mode == RuntimeMode::kMp3) {
    return "MP3";
  }
  return g_uSonFunctional ? "U-SON" : "U_LOCK";
}

enum class StartupStage : uint8_t {
  kInactive = 0,
  kBootValidation = 1,
};

enum class AppStage : uint8_t {
  kULockWaiting = 0,
  kULockListening = 1,
  kUSonFunctional = 2,
  kMp3 = 3,
};

StartupStage currentStartupStage() {
  if (g_bootAudioProtocol.active) {
    return StartupStage::kBootValidation;
  }
  return StartupStage::kInactive;
}

AppStage currentAppStage() {
  if (g_mode == RuntimeMode::kMp3) {
    return AppStage::kMp3;
  }
  if (!g_uSonFunctional) {
    return g_uLockListening ? AppStage::kULockListening : AppStage::kULockWaiting;
  }
  return AppStage::kUSonFunctional;
}

bool isUlockContext() {
  return g_mode == RuntimeMode::kSignal && !g_uSonFunctional;
}

void continueAfterBootProtocol(const char* source) {
  if (g_mode != RuntimeMode::kSignal || g_uSonFunctional || g_uLockListening) {
    return;
  }

  g_uLockListening = true;
  g_laDetectionEnabled = true;
  resetLaHoldProgress();
  g_laDetector.setCaptureEnabled(true);
  if (config::kEnableMicCalibrationOnSignalEntry) {
    startMicCalibration(millis(), source);
  }
  requestULockSearchSonarCue(source);
  Serial.printf("[MODE] U_LOCK -> detection LA activee (%s)\n", source);
}

void requestULockSearchSonarCue(const char* source) {
  if (g_uLockSearchAudioCue.active) {
    return;
  }
  g_uLockSearchAudioCue.pending = true;
  Serial.printf("[AUDIO_FX] Sonar cue queued (%s)\n", source);
}

void cancelULockSearchSonarCue(const char* source) {
  if (!g_uLockSearchAudioCue.pending && !g_uLockSearchAudioCue.active) {
    return;
  }

  if (g_uLockSearchAudioCue.active) {
    g_bootRadioScanFx.stop();
    if (g_uLockSearchAudioCue.restoreMicCapture && g_mode == RuntimeMode::kSignal &&
        g_laDetectionEnabled) {
      g_laDetector.setCaptureEnabled(true);
    }
  }

  g_uLockSearchAudioCue.pending = false;
  g_uLockSearchAudioCue.active = false;
  g_uLockSearchAudioCue.restoreMicCapture = false;
  g_uLockSearchAudioCue.untilMs = 0U;
  Serial.printf("[AUDIO_FX] Sonar cue canceled (%s)\n", source);
}

void serviceULockSearchSonarCue(uint32_t nowMs) {
  if (g_uLockSearchAudioCue.active) {
    if (g_bootAudioProtocol.active || g_mode != RuntimeMode::kSignal || g_uSonFunctional || !g_uLockListening ||
        static_cast<int32_t>(nowMs - g_uLockSearchAudioCue.untilMs) >= 0) {
      cancelULockSearchSonarCue("ulock_search_done");
      return;
    }
    g_bootRadioScanFx.update(nowMs, config::kBootRadioScanChunkMs);
    return;
  }

  if (!g_uLockSearchAudioCue.pending || g_bootAudioProtocol.active) {
    return;
  }
  if (g_mode != RuntimeMode::kSignal || g_uSonFunctional || !g_uLockListening) {
    cancelULockSearchSonarCue("ulock_search_out_of_context");
    return;
  }

  g_uLockSearchAudioCue.pending = false;
  g_uLockSearchAudioCue.restoreMicCapture =
      config::kUseI2SMicInput && g_mode == RuntimeMode::kSignal && g_laDetectionEnabled;
  if (g_uLockSearchAudioCue.restoreMicCapture) {
    g_laDetector.setCaptureEnabled(false);
  }

  g_bootRadioScanFx.setGain(config::kUnlockI2sJingleGain);
  g_bootRadioScanFx.setSampleRate(
      (config::kBootI2sNoiseSampleRateHz > 0U) ? static_cast<uint32_t>(config::kBootI2sNoiseSampleRateHz)
                                               : 22050U);
  if (!g_bootRadioScanFx.start(FmRadioScanFx::Effect::kSonar)) {
    if (g_uLockSearchAudioCue.restoreMicCapture) {
      g_laDetector.setCaptureEnabled(true);
    }
    g_uLockSearchAudioCue.restoreMicCapture = false;
    Serial.println("[AUDIO_FX] Sonar cue start failed.");
    return;
  }

  g_uLockSearchAudioCue.active = true;
  g_uLockSearchAudioCue.untilMs = nowMs + kULockSearchSonarCueMs;
  Serial.printf("[AUDIO_FX] Sonar cue start dur=%lu ms\n",
                static_cast<unsigned long>(kULockSearchSonarCueMs));
}

void printBootAudioProtocolStatus(uint32_t nowMs, const char* source) {
  if (!g_bootAudioProtocol.active) {
    Serial.printf("[BOOT_PROTO] STATUS via=%s inactive validated=%u\n",
                  source,
                  g_bootAudioProtocol.validated ? 1U : 0U);
    return;
  }

  uint32_t leftMs = 0;
  if (g_bootAudioProtocol.deadlineMs != 0U &&
      static_cast<int32_t>(g_bootAudioProtocol.deadlineMs - nowMs) > 0) {
    leftMs = g_bootAudioProtocol.deadlineMs - nowMs;
  }

  uint32_t timeoutLeftMs = 0;
  if (config::kBootAudioValidationTimeoutMs > 0U && g_bootAudioProtocol.startMs != 0U) {
    const uint32_t elapsedMs = nowMs - g_bootAudioProtocol.startMs;
    if (elapsedMs < config::kBootAudioValidationTimeoutMs) {
      timeoutLeftMs = config::kBootAudioValidationTimeoutMs - elapsedMs;
    }
  }

  Serial.printf("[BOOT_PROTO] STATUS via=%s waiting_key=1 loops=%u scan=%u left=%lus timeout_left=%lus mode=%s\n",
                source,
                static_cast<unsigned int>(g_bootAudioProtocol.replayCount),
                g_bootRadioScanFx.isActive() ? 1U : 0U,
                static_cast<unsigned long>(leftMs / 1000UL),
                static_cast<unsigned long>(timeoutLeftMs / 1000UL),
                runtimeModeLabel());
}

void finishBootAudioValidationProtocol(const char* reason, bool validated) {
  if (!g_bootAudioProtocol.active) {
    return;
  }

  stopBootRadioScan("boot_proto_finish");
  audioService().stopAll("boot_proto_finish");
  g_bootAudioProtocol.active = false;
  g_bootAudioProtocol.validated = validated;
  g_bootAudioProtocol.waitingAudio = false;
  g_bootAudioProtocol.startMs = 0;
  g_bootAudioProtocol.deadlineMs = 0;
  g_bootAudioProtocol.nextReminderMs = 0;
  g_bootAudioProtocol.cycleSourceTag[0] = '\0';
  Serial.printf("[BOOT_PROTO] DONE status=%s reason=%s loops=%u\n",
                validated ? "VALIDATED" : "BYPASS",
                reason,
                static_cast<unsigned int>(g_bootAudioProtocol.replayCount));

  if (validated) {
    continueAfterBootProtocol(reason);
  }
}

void replayBootAudioProtocolFx(uint32_t nowMs, const char* source) {
  if (!g_bootAudioProtocol.active) {
    return;
  }

  Serial.printf("[BOOT_PROTO] REPLAY via %s\n", source);
  startBootAudioLoopCycle(nowMs, source);
  printBootAudioProtocolStatus(nowMs, source);
}

void startBootAudioValidationProtocol(uint32_t nowMs) {
  if (!config::kEnableBootAudioValidationProtocol) {
    return;
  }

  g_bootAudioProtocol.active = true;
  g_bootAudioProtocol.validated = false;
  g_bootAudioProtocol.waitingAudio = false;
  g_bootAudioProtocol.replayCount = 0;
  g_bootAudioProtocol.startMs = nowMs;
  g_bootAudioProtocol.deadlineMs = 0;
  g_bootAudioProtocol.nextReminderMs = nowMs + static_cast<uint32_t>(config::kBootProtocolPromptPeriodMs);
  g_bootAudioProtocol.cycleSourceTag[0] = '\0';
  g_bootAudioProtocol.serialCmdLen = 0;
  g_bootAudioProtocol.serialCmdBuffer[0] = '\0';

  Serial.printf("[BOOT_PROTO] START timeout=%lu ms (attente touche)\n",
                static_cast<unsigned long>(config::kBootAudioValidationTimeoutMs));
  startBootAudioLoopCycle(nowMs, "boot_proto_start");
  printBootAudioProtocolStatus(nowMs, "start");
  printBootAudioProtocolHelp();
}

void processBootAudioSerialCommand(const char* rawCmd, uint32_t nowMs) {
  if (rawCmd == nullptr || rawCmd[0] == '\0') {
    return;
  }

  char cmd[32] = {};
  size_t src = 0;
  while (rawCmd[src] != '\0' && isspace(static_cast<unsigned char>(rawCmd[src])) != 0) {
    ++src;
  }

  size_t dst = 0;
  while (rawCmd[src] != '\0' && dst < (sizeof(cmd) - 1U)) {
    cmd[dst++] = static_cast<char>(toupper(static_cast<unsigned char>(rawCmd[src++])));
  }
  while (dst > 0U && isspace(static_cast<unsigned char>(cmd[dst - 1U])) != 0) {
    --dst;
  }
  cmd[dst] = '\0';

  if (dst == 0U) {
    return;
  }

  if (processCodecDebugCommand(cmd)) {
    return;
  }
  if (processStoryDebugCommand(cmd, nowMs)) {
    return;
  }
  if (processMp3DebugCommand(cmd, nowMs)) {
    return;
  }

  const bool protocolActive = g_bootAudioProtocol.active;
  const bool statusOrHelpCmd = strcmp(cmd, "BOOT_STATUS") == 0 || strcmp(cmd, "STATUS") == 0 ||
                               strcmp(cmd, "?") == 0 || strcmp(cmd, "BOOT_HELP") == 0 ||
                               strcmp(cmd, "HELP") == 0 || strcmp(cmd, "H") == 0;
  const bool paStatusCmd = strcmp(cmd, "BOOT_PA_STATUS") == 0 || strcmp(cmd, "PA") == 0;
  const bool fsInfoCmd = strcmp(cmd, "BOOT_FS_INFO") == 0 || strcmp(cmd, "FS_INFO") == 0;
  const bool fsListCmd = strcmp(cmd, "BOOT_FS_LIST") == 0 || strcmp(cmd, "FS_LIST") == 0;

  // Hors fenetre boot, les actions BOOT sont reservees au mode U_LOCK.
  // En MP3/U-SON, on autorise uniquement les commandes de lecture de statut.
  if (!protocolActive && !isUlockContext() && !statusOrHelpCmd && !paStatusCmd && !fsInfoCmd &&
      !fsListCmd) {
    Serial.printf("[BOOT_PROTO] Refuse hors U_LOCK (mode=%s): %s\n", runtimeModeLabel(), cmd);
    Serial.println(
        "[BOOT_PROTO] Autorise hors U_LOCK: BOOT_STATUS | BOOT_HELP | BOOT_PA_STATUS | BOOT_FS_INFO | BOOT_FS_LIST");
    return;
  }

  if (strcmp(cmd, "BOOT_REOPEN") == 0 || strcmp(cmd, "BOOT_REARM") == 0 || strcmp(cmd, "BOOT_START") == 0) {
    if (protocolActive) {
      Serial.println("[BOOT_PROTO] REOPEN: protocole actif, redemarre la boucle.");
      replayBootAudioProtocolFx(nowMs, "serial_boot_reopen_active");
      return;
    }
    Serial.println("[BOOT_PROTO] REOPEN: rearm protocole.");
    startBootAudioValidationProtocol(nowMs);
    return;
  }

  if (strcmp(cmd, "BOOT_NEXT") == 0 || strcmp(cmd, "NEXT") == 0 || strcmp(cmd, "BOOT_OK") == 0 ||
      strcmp(cmd, "OK") == 0 || strcmp(cmd, "VALID") == 0 || strcmp(cmd, "BOOT_SKIP") == 0 ||
      strcmp(cmd, "SKIP") == 0) {
    if (!protocolActive) {
      Serial.println("[BOOT_PROTO] BOOT_NEXT ignore: protocole inactif (utiliser BOOT_REOPEN).");
      return;
    }
    finishBootAudioValidationProtocol("serial_boot_next", true);
    return;
  }

  if (strcmp(cmd, "BOOT_REPLAY") == 0 || strcmp(cmd, "REPLAY") == 0 || strcmp(cmd, "R") == 0) {
    if (protocolActive) {
      replayBootAudioProtocolFx(nowMs, "serial_boot_replay");
    } else {
      Serial.println("[BOOT_PROTO] REPLAY hors protocole: test manuel boucle boot.");
      if (!startRandomTokenFxAsync("BOOT", "serial_boot_replay_manual", false)) {
        startBootAudioPrimaryFxAsync("serial_boot_replay_manual");
      }
      printBootAudioProtocolStatus(nowMs, "serial_boot_replay_manual");
    }
    return;
  }
  if (strcmp(cmd, "BOOT_KO") == 0 || strcmp(cmd, "KO") == 0 || strcmp(cmd, "NOK") == 0) {
    if (protocolActive) {
      Serial.println("[BOOT_PROTO] KO recu (serial), relecture intro.");
      replayBootAudioProtocolFx(nowMs, "serial_boot_ko");
    } else {
      Serial.println("[BOOT_PROTO] KO hors protocole: test manuel FX boot.");
      if (!startRandomTokenFxAsync("BOOT", "serial_boot_ko_manual", false)) {
        startBootAudioPrimaryFxAsync("serial_boot_ko_manual");
      }
      printBootAudioProtocolStatus(nowMs, "serial_boot_ko_manual");
    }
    return;
  }
  if (strcmp(cmd, "BOOT_TEST_TONE") == 0 || strcmp(cmd, "TONE") == 0) {
    if (protocolActive) {
      stopBootRadioScan("serial_test_tone");
    }
    const bool started = audioService().startBaseFx(AudioEffectId::kFmSweep,
                                                    0.30f,
                                                    900U,
                                                    "serial_test_tone");
    if (protocolActive && started) {
      g_bootAudioProtocol.waitingAudio = true;
      g_bootAudioProtocol.deadlineMs = 0U;
      snprintf(g_bootAudioProtocol.cycleSourceTag,
               sizeof(g_bootAudioProtocol.cycleSourceTag),
               "%s",
               "serial_test_tone");
    }
    extendBootAudioProtocolWindow(nowMs);
    printBootAudioProtocolStatus(nowMs, "serial_test_tone");
    return;
  }
  if (strcmp(cmd, "BOOT_TEST_DIAG") == 0 || strcmp(cmd, "DIAG") == 0) {
    if (protocolActive) {
      stopBootRadioScan("serial_test_diag");
    }
    const bool started = audioService().startBaseFx(AudioEffectId::kSonar,
                                                    0.28f,
                                                    1500U,
                                                    "serial_test_diag");
    if (protocolActive && started) {
      g_bootAudioProtocol.waitingAudio = true;
      g_bootAudioProtocol.deadlineMs = 0U;
      snprintf(g_bootAudioProtocol.cycleSourceTag,
               sizeof(g_bootAudioProtocol.cycleSourceTag),
               "%s",
               "serial_test_diag");
    }
    extendBootAudioProtocolWindow(nowMs);
    printBootAudioProtocolStatus(nowMs, "serial_test_diag");
    return;
  }
  if (strcmp(cmd, "BOOT_FX_FM") == 0 || strcmp(cmd, "FX_FM") == 0) {
    if (protocolActive) {
      stopBootRadioScan("serial_fx_fm");
    }
    const bool started = audioService().startBaseFx(AudioEffectId::kFmSweep,
                                                    config::kBootI2sNoiseGain,
                                                    kFxFmDurationMs,
                                                    "serial_fx_fm");
    if (protocolActive && started) {
      g_bootAudioProtocol.waitingAudio = true;
      g_bootAudioProtocol.deadlineMs = 0U;
      snprintf(g_bootAudioProtocol.cycleSourceTag,
               sizeof(g_bootAudioProtocol.cycleSourceTag),
               "%s",
               "serial_fx_fm");
    }
    extendBootAudioProtocolWindow(nowMs);
    printBootAudioProtocolStatus(nowMs, "serial_fx_fm");
    return;
  }
  if (strcmp(cmd, "BOOT_FX_SONAR") == 0 || strcmp(cmd, "FX_SONAR") == 0) {
    if (protocolActive) {
      stopBootRadioScan("serial_fx_sonar");
    }
    const bool started = audioService().startBaseFx(AudioEffectId::kSonar,
                                                    config::kBootI2sNoiseGain,
                                                    kFxSonarDurationMs,
                                                    "serial_fx_sonar");
    if (protocolActive && started) {
      g_bootAudioProtocol.waitingAudio = true;
      g_bootAudioProtocol.deadlineMs = 0U;
      snprintf(g_bootAudioProtocol.cycleSourceTag,
               sizeof(g_bootAudioProtocol.cycleSourceTag),
               "%s",
               "serial_fx_sonar");
    }
    extendBootAudioProtocolWindow(nowMs);
    printBootAudioProtocolStatus(nowMs, "serial_fx_sonar");
    return;
  }
  if (strcmp(cmd, "BOOT_FX_MORSE") == 0 || strcmp(cmd, "FX_MORSE") == 0) {
    if (protocolActive) {
      stopBootRadioScan("serial_fx_morse");
    }
    const bool started = audioService().startBaseFx(AudioEffectId::kMorse,
                                                    config::kUnlockI2sJingleGain,
                                                    kFxMorseDurationMs,
                                                    "serial_fx_morse");
    if (protocolActive && started) {
      g_bootAudioProtocol.waitingAudio = true;
      g_bootAudioProtocol.deadlineMs = 0U;
      snprintf(g_bootAudioProtocol.cycleSourceTag,
               sizeof(g_bootAudioProtocol.cycleSourceTag),
               "%s",
               "serial_fx_morse");
    }
    extendBootAudioProtocolWindow(nowMs);
    printBootAudioProtocolStatus(nowMs, "serial_fx_morse");
    return;
  }
  if (strcmp(cmd, "BOOT_FX_WIN") == 0 || strcmp(cmd, "FX_WIN") == 0) {
    if (protocolActive) {
      stopBootRadioScan("serial_fx_win");
    }
    const bool started = audioService().startBaseFx(AudioEffectId::kWin,
                                                    config::kUnlockI2sJingleGain,
                                                    kFxWinDurationMs,
                                                    "serial_fx_win");
    if (protocolActive && started) {
      g_bootAudioProtocol.waitingAudio = true;
      g_bootAudioProtocol.deadlineMs = 0U;
      snprintf(g_bootAudioProtocol.cycleSourceTag,
               sizeof(g_bootAudioProtocol.cycleSourceTag),
               "%s",
               "serial_fx_win");
    }
    extendBootAudioProtocolWindow(nowMs);
    printBootAudioProtocolStatus(nowMs, "serial_fx_win");
    return;
  }
  if (strcmp(cmd, "BOOT_PA_ON") == 0 || strcmp(cmd, "PAON") == 0) {
    setBootAudioPaEnabled(true, "serial_pa_on");
    printBootAudioOutputInfo("serial_pa_on");
    return;
  }
  if (strcmp(cmd, "BOOT_PA_OFF") == 0 || strcmp(cmd, "PAOFF") == 0) {
    setBootAudioPaEnabled(false, "serial_pa_off");
    printBootAudioOutputInfo("serial_pa_off");
    return;
  }
  if (strcmp(cmd, "BOOT_PA_STATUS") == 0 || strcmp(cmd, "PA") == 0) {
    printBootAudioOutputInfo("serial_pa_status");
    return;
  }
  if (strcmp(cmd, "BOOT_PA_INV") == 0 || strcmp(cmd, "PAINV") == 0) {
    g_paEnableActiveHigh = !g_paEnableActiveHigh;
    Serial.printf("[AUDIO_DBG] serial_pa_inv polarity=%s\n",
                  g_paEnableActiveHigh ? "ACTIVE_HIGH" : "ACTIVE_LOW");
    setBootAudioPaEnabled(g_paEnabledRequest, "serial_pa_inv");
    printBootAudioOutputInfo("serial_pa_inv");
    return;
  }
  if (strcmp(cmd, "BOOT_FS_INFO") == 0 || strcmp(cmd, "FS_INFO") == 0) {
    printLittleFsInfo("serial_boot_fs_info");
    return;
  }
  if (strcmp(cmd, "BOOT_FS_LIST") == 0 || strcmp(cmd, "FS_LIST") == 0) {
    listLittleFsRoot("serial_boot_fs_list");
    return;
  }
  if (strcmp(cmd, "BOOT_FS_TEST") == 0 || strcmp(cmd, "FSTEST") == 0) {
    if (protocolActive) {
      stopBootRadioScan("serial_boot_fs_test");
    }
    String path;
    bool started = false;
    if (resolveBootLittleFsPath(&path)) {
      started = startAudioFromFsAsync(LittleFS,
                                      path.c_str(),
                                      config::kBootFxLittleFsGain,
                                      config::kBootFxLittleFsMaxDurationMs,
                                      "serial_boot_fs_test");
    }
    if (!started) {
      started = startBootAudioPrimaryFxAsync("serial_boot_fs_test");
    }
    if (protocolActive && started) {
      g_bootAudioProtocol.waitingAudio = true;
      g_bootAudioProtocol.deadlineMs = 0U;
      snprintf(g_bootAudioProtocol.cycleSourceTag,
               sizeof(g_bootAudioProtocol.cycleSourceTag),
               "%s",
               "serial_boot_fs_test");
    }
    return;
  }
  if (strcmp(cmd, "BOOT_STATUS") == 0 || strcmp(cmd, "STATUS") == 0 || strcmp(cmd, "?") == 0) {
    printBootAudioProtocolStatus(nowMs, "serial_boot_status");
    return;
  }
  if (strcmp(cmd, "BOOT_HELP") == 0 || strcmp(cmd, "HELP") == 0 || strcmp(cmd, "H") == 0) {
    printBootAudioProtocolHelp();
    return;
  }

  Serial.printf("[BOOT_PROTO] Commande inconnue: %s\n", cmd);
}

void pollBootAudioProtocolSerial(uint32_t nowMs) {
  if (!g_bootAudioProtocol.active) {
    return;
  }

  while (Serial.available() > 0) {
    const char c = static_cast<char>(Serial.read());
    if (c == '\r') {
      continue;
    }

    if (c == '\n') {
      g_bootAudioProtocol.serialCmdBuffer[g_bootAudioProtocol.serialCmdLen] = '\0';
      processBootAudioSerialCommand(g_bootAudioProtocol.serialCmdBuffer, nowMs);
      g_bootAudioProtocol.serialCmdLen = 0;
      continue;
    }

    if (g_bootAudioProtocol.serialCmdLen < (sizeof(g_bootAudioProtocol.serialCmdBuffer) - 1U)) {
      g_bootAudioProtocol.serialCmdBuffer[g_bootAudioProtocol.serialCmdLen++] = c;
    } else {
      g_bootAudioProtocol.serialCmdLen = 0;
    }
  }
}

void handleBootAudioProtocolKey(uint8_t key, uint32_t nowMs) {
  (void)nowMs;
  if (!g_bootAudioProtocol.active) {
    return;
  }

  switch (key) {
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
    case 6:
      Serial.printf("[BOOT_PROTO] K%u -> U_LOCK ecoute\n", static_cast<unsigned int>(key));
      finishBootAudioValidationProtocol("key_next", true);
      break;
    default:
      Serial.printf("[BOOT_PROTO] K%u ignoree (attendu K1/K2/K3/K4/K5/K6)\n",
                    static_cast<unsigned int>(key));
      break;
  }
}

void updateBootAudioValidationProtocol(uint32_t nowMs) {
  if (!g_bootAudioProtocol.active) {
    return;
  }

  if (config::kBootAudioValidationTimeoutMs > 0U && g_bootAudioProtocol.startMs != 0U &&
      static_cast<uint32_t>(nowMs - g_bootAudioProtocol.startMs) >= config::kBootAudioValidationTimeoutMs) {
    Serial.printf("[BOOT_PROTO] Timeout auto atteint (%lu ms) -> passage U_LOCK ecoute.\n",
                  static_cast<unsigned long>(config::kBootAudioValidationTimeoutMs));
    finishBootAudioValidationProtocol("timeout_auto", true);
    return;
  }

  updateAsyncAudioService(nowMs);
  if (!g_bootAudioProtocol.active) {
    return;
  }

  if (g_bootAudioProtocol.waitingAudio) {
    if (audioService().isBaseBusy()) {
      return;
    }
    g_bootAudioProtocol.waitingAudio = false;

    const char* cycleSource =
        (g_bootAudioProtocol.cycleSourceTag[0] != '\0') ? g_bootAudioProtocol.cycleSourceTag
                                                        : "boot_proto_audio_done";
    if (!startBootRadioScan(cycleSource)) {
      g_bootAudioProtocol.deadlineMs = nowMs + 5000U;
      Serial.println("[BOOT_PROTO] Radio scan KO, retry auto dans 5s.");
      return;
    }
    armBootAudioLoopScanWindow(nowMs, cycleSource);
    g_bootAudioProtocol.nextReminderMs =
        nowMs + static_cast<uint32_t>(config::kBootProtocolPromptPeriodMs);
    return;
  }

  updateBootRadioScan(nowMs);
  if (!g_bootAudioProtocol.active) {
    return;
  }

  if (!g_bootRadioScanFx.isActive()) {
    if (g_bootAudioProtocol.deadlineMs == 0U ||
        static_cast<int32_t>(nowMs - g_bootAudioProtocol.deadlineMs) >= 0) {
      startBootAudioLoopCycle(nowMs, "boot_proto_recover");
      if (!g_bootAudioProtocol.active) {
        return;
      }
    }
  } else if (g_bootAudioProtocol.deadlineMs != 0U &&
             static_cast<int32_t>(nowMs - g_bootAudioProtocol.deadlineMs) >= 0) {
    startBootAudioLoopCycle(nowMs, "boot_proto_cycle");
    if (!g_bootAudioProtocol.active) {
      return;
    }
  }

  if (static_cast<int32_t>(nowMs - g_bootAudioProtocol.nextReminderMs) >= 0) {
    printBootAudioProtocolStatus(nowMs, "tick");
    Serial.println("[BOOT_PROTO] Attente touche: K1..K6 pour lancer U_LOCK ecoute.");
    g_bootAudioProtocol.nextReminderMs =
        nowMs + static_cast<uint32_t>(config::kBootProtocolPromptPeriodMs);
  }
}

void printKeyTuneThresholds(const char* source) {
  const KeypadAnalog::Thresholds& thresholds = g_keypad.thresholds();
  Serial.printf("[KEY_TUNE] %s rel=%u k1=%u k2=%u k3=%u k4=%u k5=%u k6=%u\n",
                source,
                static_cast<unsigned int>(thresholds.releaseThreshold),
                static_cast<unsigned int>(thresholds.keyMax[0]),
                static_cast<unsigned int>(thresholds.keyMax[1]),
                static_cast<unsigned int>(thresholds.keyMax[2]),
                static_cast<unsigned int>(thresholds.keyMax[3]),
                static_cast<unsigned int>(thresholds.keyMax[4]),
                static_cast<unsigned int>(thresholds.keyMax[5]));
}

void resetKeySelfTestStats() {
  g_keySelfTest.seenCount = 0;
  for (uint8_t i = 0; i < 6U; ++i) {
    g_keySelfTest.seen[i] = false;
    g_keySelfTest.rawMin[i] = 0xFFFF;
    g_keySelfTest.rawMax[i] = 0;
  }
}

void printKeySelfTestStatus(const char* source) {
  auto minValue = [](uint16_t value) -> uint16_t { return (value == 0xFFFF) ? 0U : value; };
  Serial.printf(
      "[KEY_TEST] %s active=%u seen=%u/6 K1=%u(%u..%u) K2=%u(%u..%u) K3=%u(%u..%u) "
      "K4=%u(%u..%u) K5=%u(%u..%u) K6=%u(%u..%u)\n",
      source,
      g_keySelfTest.active ? 1U : 0U,
      static_cast<unsigned int>(g_keySelfTest.seenCount),
      g_keySelfTest.seen[0] ? 1U : 0U,
      static_cast<unsigned int>(minValue(g_keySelfTest.rawMin[0])),
      static_cast<unsigned int>(g_keySelfTest.rawMax[0]),
      g_keySelfTest.seen[1] ? 1U : 0U,
      static_cast<unsigned int>(minValue(g_keySelfTest.rawMin[1])),
      static_cast<unsigned int>(g_keySelfTest.rawMax[1]),
      g_keySelfTest.seen[2] ? 1U : 0U,
      static_cast<unsigned int>(minValue(g_keySelfTest.rawMin[2])),
      static_cast<unsigned int>(g_keySelfTest.rawMax[2]),
      g_keySelfTest.seen[3] ? 1U : 0U,
      static_cast<unsigned int>(minValue(g_keySelfTest.rawMin[3])),
      static_cast<unsigned int>(g_keySelfTest.rawMax[3]),
      g_keySelfTest.seen[4] ? 1U : 0U,
      static_cast<unsigned int>(minValue(g_keySelfTest.rawMin[4])),
      static_cast<unsigned int>(g_keySelfTest.rawMax[4]),
      g_keySelfTest.seen[5] ? 1U : 0U,
      static_cast<unsigned int>(minValue(g_keySelfTest.rawMin[5])),
      static_cast<unsigned int>(g_keySelfTest.rawMax[5]));
}

void startKeySelfTest() {
  g_keySelfTest.active = true;
  g_keyTune.rawStreamEnabled = false;
  resetKeySelfTestStats();
  Serial.println("[KEY_TEST] START: appuyer K1..K6 (une fois chacun).");
  printKeySelfTestStatus("start");
}

void stopKeySelfTest(const char* reason) {
  if (!g_keySelfTest.active) {
    return;
  }
  g_keySelfTest.active = false;
  printKeySelfTestStatus(reason);
}

void handleKeySelfTestPress(uint8_t key, uint16_t raw) {
  if (!g_keySelfTest.active) {
    return;
  }
  if (key < 1U || key > 6U) {
    Serial.printf("[KEY_TEST] key invalide=%u raw=%u\n",
                  static_cast<unsigned int>(key),
                  static_cast<unsigned int>(raw));
    return;
  }

  const uint8_t idx = static_cast<uint8_t>(key - 1U);
  const bool wasSeen = g_keySelfTest.seen[idx];
  if (!wasSeen) {
    g_keySelfTest.seen[idx] = true;
    ++g_keySelfTest.seenCount;
  }

  if (raw < g_keySelfTest.rawMin[idx]) {
    g_keySelfTest.rawMin[idx] = raw;
  }
  if (raw > g_keySelfTest.rawMax[idx]) {
    g_keySelfTest.rawMax[idx] = raw;
  }

  Serial.printf("[KEY_TEST] HIT K%u raw=%u %s seen=%u/6\n",
                static_cast<unsigned int>(key),
                static_cast<unsigned int>(raw),
                wasSeen ? "again" : "new",
                static_cast<unsigned int>(g_keySelfTest.seenCount));

  if (g_keySelfTest.seenCount >= 6U) {
    Serial.println("[KEY_TEST] SUCCESS: K1..K6 valides.");
    stopKeySelfTest("done");
  }
}

void printCodecDebugHelp() {
  Serial.println("[CODEC] Cmd: CODEC_STATUS | CODEC_DUMP [from to]");
  Serial.println("[CODEC] Cmd: CODEC_RD reg | CODEC_WR reg val");
  Serial.println("[CODEC] Cmd: CODEC_VOL 0..100 | CODEC_VOL_RAW 0..0x21 [out2=0|1]");
}

bool processCodecDebugCommand(const char* cmd) {
  if (cmd == nullptr || cmd[0] == '\0') {
    return false;
  }

  if (strcmp(cmd, "CODEC_HELP") == 0 || strcmp(cmd, "CHELP") == 0) {
    printCodecDebugHelp();
    return true;
  }

  if (strcmp(cmd, "CODEC_STATUS") == 0 || strcmp(cmd, "CSTAT") == 0) {
    const bool readyBefore = g_laDetector.isCodecReady();
    const uint8_t addrBefore = g_laDetector.codecAddress();
    Serial.printf("[CODEC] status ready=%u addr=0x%02X sda=%u scl=%u i2s_mic=%u\n",
                  readyBefore ? 1U : 0U,
                  static_cast<unsigned int>(addrBefore),
                  static_cast<unsigned int>(config::kPinCodecI2CSda),
                  static_cast<unsigned int>(config::kPinCodecI2CScl),
                  config::kUseI2SMicInput ? 1U : 0U);
    if (!g_laDetector.ensureCodecReady()) {
      Serial.println("[CODEC] ensure failed (codec absent ou I2C NOK).");
      return true;
    }

    uint8_t v2e = 0;
    uint8_t v2f = 0;
    uint8_t v30 = 0;
    uint8_t v31 = 0;
    const bool ok = g_laDetector.readCodecRegister(0x2E, &v2e) &&
                    g_laDetector.readCodecRegister(0x2F, &v2f) &&
                    g_laDetector.readCodecRegister(0x30, &v30) &&
                    g_laDetector.readCodecRegister(0x31, &v31);
    if (ok) {
      Serial.printf("[CODEC] out_vol raw L1=0x%02X R1=0x%02X L2=0x%02X R2=0x%02X\n",
                    static_cast<unsigned int>(v2e),
                    static_cast<unsigned int>(v2f),
                    static_cast<unsigned int>(v30),
                    static_cast<unsigned int>(v31));
    } else {
      Serial.println("[CODEC] out_vol read failed.");
    }
    return true;
  }

  int from = 0;
  int to = 0;
  const bool dumpDefault = strcmp(cmd, "CODEC_DUMP") == 0 || strcmp(cmd, "CDUMP") == 0;
  if (dumpDefault ||
      sscanf(cmd, "CODEC_DUMP %i %i", &from, &to) == 2 ||
      sscanf(cmd, "CDUMP %i %i", &from, &to) == 2) {
    if (!g_laDetector.ensureCodecReady()) {
      Serial.println("[CODEC] dump failed: codec non pret.");
      return true;
    }

    if (dumpDefault) {
      static const uint8_t kDefaultRegs[] = {
          0x00, 0x01, 0x02, 0x03, 0x04, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D,
          0x10, 0x11, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x26, 0x27, 0x2A, 0x2B, 0x2D, 0x2E,
          0x2F, 0x30, 0x31};
      Serial.println("[CODEC] dump (default regs):");
      for (uint8_t i = 0; i < (sizeof(kDefaultRegs) / sizeof(kDefaultRegs[0])); ++i) {
        uint8_t value = 0;
        if (g_laDetector.readCodecRegister(kDefaultRegs[i], &value)) {
          Serial.printf("[CODEC]   reg 0x%02X = 0x%02X\n",
                        static_cast<unsigned int>(kDefaultRegs[i]),
                        static_cast<unsigned int>(value));
        } else {
          Serial.printf("[CODEC]   reg 0x%02X = <ERR>\n",
                        static_cast<unsigned int>(kDefaultRegs[i]));
        }
      }
      return true;
    }

    if (from < 0 || from > 0xFF || to < 0 || to > 0xFF || to < from) {
      Serial.println("[CODEC] CODEC_DUMP invalide: utiliser from<=to dans [0..255].");
      return true;
    }

    Serial.printf("[CODEC] dump range 0x%02X..0x%02X\n",
                  static_cast<unsigned int>(from),
                  static_cast<unsigned int>(to));
    for (int reg = from; reg <= to; ++reg) {
      uint8_t value = 0;
      if (g_laDetector.readCodecRegister(static_cast<uint8_t>(reg), &value)) {
        Serial.printf("[CODEC]   reg 0x%02X = 0x%02X\n",
                      static_cast<unsigned int>(reg),
                      static_cast<unsigned int>(value));
      } else {
        Serial.printf("[CODEC]   reg 0x%02X = <ERR>\n",
                      static_cast<unsigned int>(reg));
      }
    }
    return true;
  }

  int reg = 0;
  if (sscanf(cmd, "CODEC_RD %i", &reg) == 1 || sscanf(cmd, "CRD %i", &reg) == 1) {
    if (reg < 0 || reg > 0xFF) {
      Serial.println("[CODEC] CODEC_RD invalide: reg [0..255].");
      return true;
    }
    uint8_t value = 0;
    if (g_laDetector.readCodecRegister(static_cast<uint8_t>(reg), &value)) {
      Serial.printf("[CODEC] RD reg=0x%02X val=0x%02X (%u)\n",
                    static_cast<unsigned int>(reg),
                    static_cast<unsigned int>(value),
                    static_cast<unsigned int>(value));
    } else {
      Serial.printf("[CODEC] RD failed reg=0x%02X\n",
                    static_cast<unsigned int>(reg));
    }
    return true;
  }

  int value = 0;
  if (sscanf(cmd, "CODEC_WR %i %i", &reg, &value) == 2 ||
      sscanf(cmd, "CWR %i %i", &reg, &value) == 2) {
    if (reg < 0 || reg > 0xFF || value < 0 || value > 0xFF) {
      Serial.println("[CODEC] CODEC_WR invalide: reg/val [0..255].");
      return true;
    }
    const bool ok = g_laDetector.writeCodecRegister(static_cast<uint8_t>(reg),
                                                    static_cast<uint8_t>(value));
    Serial.printf("[CODEC] WR reg=0x%02X val=0x%02X %s\n",
                  static_cast<unsigned int>(reg),
                  static_cast<unsigned int>(value),
                  ok ? "OK" : "ERR");
    return true;
  }

  int percent = 0;
  if (sscanf(cmd, "CODEC_VOL %d", &percent) == 1 || sscanf(cmd, "CVOL %d", &percent) == 1) {
    if (percent < 0 || percent > 100) {
      Serial.println("[CODEC] CODEC_VOL invalide: 0..100.");
      return true;
    }

    const uint8_t raw = LaDetector::codecOutputRawFromPercent(static_cast<uint8_t>(percent));
    const bool ok = g_laDetector.setCodecOutputVolumeRaw(raw, true);
    g_mp3.setGain(static_cast<float>(percent) / 100.0f);
    Serial.printf("[CODEC] VOL pct=%u raw=0x%02X codec=%s mp3_gain=%u%%\n",
                  static_cast<unsigned int>(percent),
                  static_cast<unsigned int>(raw),
                  ok ? "OK" : "ERR",
                  static_cast<unsigned int>(g_mp3.volumePercent()));
    return true;
  }

  int raw = 0;
  int includeOut2 = 1;
  if (sscanf(cmd, "CODEC_VOL_RAW %i %i", &raw, &includeOut2) >= 1 ||
      sscanf(cmd, "CVRAW %i %i", &raw, &includeOut2) >= 1) {
    if (raw < 0 || raw > 0x21) {
      Serial.println("[CODEC] CODEC_VOL_RAW invalide: 0..0x21.");
      return true;
    }
    const bool ok =
        g_laDetector.setCodecOutputVolumeRaw(static_cast<uint8_t>(raw), includeOut2 != 0);
    Serial.printf("[CODEC] VOL_RAW raw=0x%02X out2=%u %s\n",
                  static_cast<unsigned int>(raw),
                  includeOut2 != 0 ? 1U : 0U,
                  ok ? "OK" : "ERR");
    return true;
  }

  return false;
}

const char* mp3FxModeLabel(Mp3FxMode mode) {
  return (mode == Mp3FxMode::kDucking) ? "DUCKING" : "OVERLAY";
}

const char* mp3FxEffectLabel(Mp3FxEffect effect) {
  return audioEffectLabel(effect);
}

bool parseMp3FxEffectToken(const char* token, Mp3FxEffect* outEffect) {
  return parseAudioEffectToken(token, outEffect);
}

bool triggerMp3Fx(Mp3FxEffect effect, uint32_t durationMs, const char* source) {
  if (durationMs == 0U) {
    durationMs = config::kMp3FxDefaultDurationMs;
  }
  if (durationMs < 250U) {
    durationMs = 250U;
  } else if (durationMs > 12000U) {
    durationMs = 12000U;
  }

  if (!g_mp3.isPlaying()) {
    Serial.printf("[MP3_FX] %s refuse: MP3 non actif.\n", source);
    return false;
  }

  const bool ok =
      audioService().startOverlayFx(effect, g_mp3.fxOverlayGain(), durationMs, source);
  Serial.printf("[MP3_FX] %s effect=%s mode=%s duck=%u%% mix=%u%% dur=%lu ms %s\n",
                source,
                mp3FxEffectLabel(effect),
                mp3FxModeLabel(g_mp3.fxMode()),
                static_cast<unsigned int>(g_mp3.fxDuckingGain() * 100.0f),
                static_cast<unsigned int>(g_mp3.fxOverlayGain() * 100.0f),
                static_cast<unsigned long>(durationMs),
                ok ? "OK" : "KO");
  return ok;
}

void printMp3DebugHelp() {
  Serial.println("[MP3_DBG] Cmd: MP3_STATUS | MP3_UNLOCK | MP3_REFRESH | MP3_LIST");
  Serial.println("[MP3_DBG] Cmd: MP3_NEXT | MP3_PREV | MP3_RESTART | MP3_PLAY n");
  Serial.println("[MP3_DBG] Cmd: MP3_TEST_START [ms] | MP3_TEST_STOP");
  Serial.println("[MP3_DBG] Cmd: MP3_FX_MODE DUCK|OVERLAY | MP3_FX_GAIN duck% mix%");
  Serial.println("[MP3_DBG] Cmd: MP3_FX FM|SONAR|MORSE|WIN [ms] | MP3_FX_STOP");
  Serial.println("[MP3_DBG] Cmd: MP3_BACKEND STATUS|SET AUTO|AUDIO_TOOLS|LEGACY");
  Serial.println("[MP3_DBG] Cmd: MP3_SCAN START|STATUS|CANCEL|REBUILD");
  Serial.println("[MP3_DBG] Cmd: MP3_BROWSE LS [path] | MP3_BROWSE CD <path> | MP3_PLAY_PATH <path>");
  Serial.println("[MP3_DBG] Cmd: MP3_UI PAGE NOW|BROWSE|QUEUE|SET | MP3_STATE SAVE|LOAD|RESET");
}

void stopMp3FormatTest(const char* reason) {
  if (!g_mp3FormatTest.active) {
    return;
  }
  g_mp3FormatTest.active = false;
  Serial.printf("[MP3_TEST] STOP reason=%s tested=%u ok=%u fail=%u total=%u\n",
                reason,
                static_cast<unsigned int>(g_mp3FormatTest.testedTracks),
                static_cast<unsigned int>(g_mp3FormatTest.okTracks),
                static_cast<unsigned int>(g_mp3FormatTest.failTracks),
                static_cast<unsigned int>(g_mp3FormatTest.totalTracks));
}

void forceUsonFunctionalForMp3Debug(const char* source) {
  if (g_uSonFunctional) {
    return;
  }
  g_uSonFunctional = true;
  g_uLockListening = false;
  g_laDetectionEnabled = false;
  g_laDetector.setCaptureEnabled(false);
  resetLaHoldProgress();
  Serial.printf("[MP3_DBG] %s force unlock -> MODULE U-SON Fonctionnel.\n", source);
}

void printMp3Status(const char* source) {
  const String current = g_mp3.currentTrackName();
  const CatalogStats stats = g_mp3.catalogStats();
  const PlayerUiPage page = currentPlayerUiPage();
  Serial.printf(
      "[MP3_DBG] %s mode=%s u_son=%u sd=%u tracks=%u cur=%u play=%u pause=%u repeat=%s vol=%u%% fx_mode=%s fx=%u(%s,%lums) duck=%u%% mix=%u%% backend=%s/%s err=%s scan_busy=%u scan_ms=%lu ui=%s browse=%s file=%s\n",
      source,
      runtimeModeLabel(),
      g_uSonFunctional ? 1U : 0U,
      g_mp3.isSdReady() ? 1U : 0U,
      static_cast<unsigned int>(g_mp3.trackCount()),
      static_cast<unsigned int>(g_mp3.currentTrackNumber()),
      g_mp3.isPlaying() ? 1U : 0U,
      g_mp3.isPaused() ? 1U : 0U,
      g_mp3.repeatModeLabel(),
      static_cast<unsigned int>(g_mp3.volumePercent()),
      g_mp3.fxModeLabel(),
      g_mp3.isFxActive() ? 1U : 0U,
      g_mp3.fxEffectLabel(),
      static_cast<unsigned long>(g_mp3.fxRemainingMs()),
      static_cast<unsigned int>(g_mp3.fxDuckingGain() * 100.0f),
      static_cast<unsigned int>(g_mp3.fxOverlayGain() * 100.0f),
      g_mp3.backendModeLabel(),
      g_mp3.activeBackendLabel(),
      g_mp3.lastBackendError(),
      g_mp3.isScanBusy() ? 1U : 0U,
      static_cast<unsigned long>(stats.scanMs),
      playerUiPageLabel(page),
      currentBrowsePath(),
      current.isEmpty() ? "-" : current.c_str());
  if (g_mp3FormatTest.active) {
    Serial.printf("[MP3_TEST] active tested=%u/%u ok=%u fail=%u dwell=%lu ms\n",
                  static_cast<unsigned int>(g_mp3FormatTest.testedTracks),
                  static_cast<unsigned int>(g_mp3FormatTest.totalTracks),
                  static_cast<unsigned int>(g_mp3FormatTest.okTracks),
                  static_cast<unsigned int>(g_mp3FormatTest.failTracks),
                  static_cast<unsigned long>(g_mp3FormatTest.dwellMs));
  }
}

void printMp3SupportedSdList(uint32_t nowMs, const char* source) {
  g_mp3.requestStorageRefresh();
  g_mp3.update(nowMs, g_mode == RuntimeMode::kMp3);
  if (!g_mp3.isSdReady()) {
    Serial.printf("[MP3_DBG] %s list refused: SD non montee.\n", source);
    return;
  }
  printMp3BrowseList(source, currentBrowsePath(), 0U, 24U);
}

bool processMp3DebugCommand(const char* cmd, uint32_t nowMs) {
  if (cmd == nullptr || cmd[0] == '\0') {
    return false;
  }

  if (strcmp(cmd, "MP3_HELP") == 0 || strcmp(cmd, "MHELP") == 0) {
    printMp3DebugHelp();
    return true;
  }

  if (strcmp(cmd, "MP3_STATUS") == 0 || strcmp(cmd, "MSTAT") == 0) {
    printMp3Status("status");
    return true;
  }

  if (commandMatches(cmd, "MP3_BACKEND")) {
    const char* arg = cmd + strlen("MP3_BACKEND");
    while (*arg == ' ') {
      ++arg;
    }

    if (arg[0] == '\0' || strcmp(arg, "STATUS") == 0) {
      Serial.printf("[MP3_BACKEND] mode=%s active=%s err=%s\n",
                    g_mp3.backendModeLabel(),
                    g_mp3.activeBackendLabel(),
                    g_mp3.lastBackendError());
      return true;
    }

    char modeToken[24] = {};
    if (sscanf(arg, "SET %23s", modeToken) == 1) {
      PlayerBackendMode mode = PlayerBackendMode::kAutoFallback;
      if (!parseBackendModeToken(modeToken, &mode)) {
        Serial.printf("[MP3_BACKEND] BAD_ARGS mode=%s (AUTO|AUDIO_TOOLS|LEGACY)\n", modeToken);
        return true;
      }
      g_mp3.setBackendMode(mode);
      Serial.printf("[MP3_BACKEND] SET mode=%s\n", g_mp3.backendModeLabel());
      printMp3Status("backend_set");
      return true;
    }

    Serial.printf("[MP3_BACKEND] BAD_ARGS cmd=%s\n", cmd);
    return true;
  }

  if (commandMatches(cmd, "MP3_SCAN")) {
    const char* arg = cmd + strlen("MP3_SCAN");
    while (*arg == ' ') {
      ++arg;
    }
    if (arg[0] == '\0' || strcmp(arg, "STATUS") == 0) {
      printMp3ScanStatus("status");
      return true;
    }
    if (strcmp(arg, "START") == 0 || strcmp(arg, "REBUILD") == 0) {
      g_mp3.requestStorageRefresh();
      g_mp3.update(nowMs, g_mode == RuntimeMode::kMp3);
      printMp3ScanStatus(strcmp(arg, "REBUILD") == 0 ? "rebuild" : "start");
      return true;
    }
    if (strcmp(arg, "CANCEL") == 0) {
      Serial.println("[MP3_SCAN] OUT_OF_CONTEXT scan is synchronous.");
      return true;
    }
    Serial.printf("[MP3_SCAN] BAD_ARGS op=%s (START|STATUS|CANCEL|REBUILD)\n", arg);
    return true;
  }

  if (commandMatches(cmd, "MP3_BROWSE")) {
    const char* arg = cmd + strlen("MP3_BROWSE");
    while (*arg == ' ') {
      ++arg;
    }
    if (strncmp(arg, "LS", 2) == 0 && (arg[2] == '\0' || arg[2] == ' ')) {
      const char* path = arg + 2;
      while (*path == ' ') {
        ++path;
      }
      printMp3BrowseList("ls", (path[0] == '\0') ? currentBrowsePath() : path, 0U, 12U);
      return true;
    }
    if (strncmp(arg, "CD", 2) == 0 && (arg[2] == ' ' || arg[2] == '\0')) {
      const char* path = arg + 2;
      while (*path == ' ') {
        ++path;
      }
      if (path[0] == '\0') {
        Serial.println("[MP3_BROWSE] BAD_ARGS path required");
        return true;
      }
      String normalizedPath(path);
      if (!normalizedPath.startsWith("/")) {
        normalizedPath = "/" + normalizedPath;
      }
      const uint16_t count = g_mp3.countTracks(normalizedPath.c_str());
      if (count == 0U) {
        Serial.printf("[MP3_BROWSE] NOT_FOUND path=%s\n", normalizedPath.c_str());
        return true;
      }
      g_mp3BrowsePath = normalizedPath;
      g_playerUi.setPage(PlayerUiPage::kBrowser);
      Serial.printf("[MP3_BROWSE] CD path=%s count=%u\n",
                    g_mp3BrowsePath.c_str(),
                    static_cast<unsigned int>(count));
      return true;
    }
    Serial.printf("[MP3_BROWSE] BAD_ARGS cmd=%s\n", cmd);
    return true;
  }

  if (commandMatches(cmd, "MP3_PLAY_PATH")) {
    const char* path = cmd + strlen("MP3_PLAY_PATH");
    while (*path == ' ') {
      ++path;
    }
    if (path[0] == '\0') {
      Serial.println("[MP3_DBG] BAD_ARGS MP3_PLAY_PATH <path>");
      return true;
    }
    if (!g_mp3.playPath(path)) {
      Serial.printf("[MP3_DBG] NOT_FOUND path=%s\n", path);
      return true;
    }
    printMp3Status("play_path");
    return true;
  }

  if (commandMatches(cmd, "MP3_UI")) {
    const char* arg = cmd + strlen("MP3_UI");
    while (*arg == ' ') {
      ++arg;
    }
    if (arg[0] == '\0' || strcmp(arg, "STATUS") == 0) {
      Serial.printf("[MP3_UI] page=%s cursor=%u offset=%u\n",
                    playerUiPageLabel(g_playerUi.page()),
                    static_cast<unsigned int>(g_playerUi.cursor()),
                    static_cast<unsigned int>(g_playerUi.offset()));
      return true;
    }

    char pageToken[16] = {};
    if (sscanf(arg, "PAGE %15s", pageToken) == 1) {
      PlayerUiPage page = PlayerUiPage::kNowPlaying;
      if (!parsePlayerUiPageToken(pageToken, &page)) {
        Serial.printf("[MP3_UI] BAD_ARGS page=%s (NOW|BROWSE|QUEUE|SET)\n", pageToken);
        return true;
      }
      setPlayerUiPage(page);
      Serial.printf("[MP3_UI] PAGE %s\n", playerUiPageLabel(g_playerUi.page()));
      return true;
    }

    Serial.printf("[MP3_UI] BAD_ARGS cmd=%s\n", cmd);
    return true;
  }

  if (commandMatches(cmd, "MP3_STATE")) {
    const char* arg = cmd + strlen("MP3_STATE");
    while (*arg == ' ') {
      ++arg;
    }
    if (strcmp(arg, "SAVE") == 0) {
      Serial.printf("[MP3_STATE] SAVE %s\n", g_mp3.savePlayerState() ? "OK" : "FAILED");
      return true;
    }
    if (strcmp(arg, "LOAD") == 0) {
      const bool ok = g_mp3.loadPlayerState();
      if (ok) {
        g_mp3.requestStorageRefresh();
        g_mp3.update(nowMs, g_mode == RuntimeMode::kMp3);
      }
      Serial.printf("[MP3_STATE] LOAD %s\n", ok ? "OK" : "FAILED");
      return true;
    }
    if (strcmp(arg, "RESET") == 0) {
      Serial.printf("[MP3_STATE] RESET %s\n", g_mp3.resetPlayerState() ? "OK" : "FAILED");
      return true;
    }
    Serial.printf("[MP3_STATE] BAD_ARGS op=%s (SAVE|LOAD|RESET)\n", arg);
    return true;
  }

  if (strcmp(cmd, "MP3_UNLOCK") == 0 || strcmp(cmd, "MUNLOCK") == 0) {
    forceUsonFunctionalForMp3Debug("serial_mp3_unlock");
    g_mp3.requestStorageRefresh();
    g_mp3.update(nowMs, false);
    printMp3Status("unlock");
    return true;
  }

  if (strcmp(cmd, "MP3_REFRESH") == 0 || strcmp(cmd, "MREFRESH") == 0) {
    g_mp3.requestStorageRefresh();
    g_mp3.update(nowMs, g_mode == RuntimeMode::kMp3);
    printMp3Status("refresh");
    return true;
  }

  if (strcmp(cmd, "MP3_LIST") == 0 || strcmp(cmd, "MLIST") == 0) {
    printMp3SupportedSdList(nowMs, "list");
    return true;
  }

  if (strcmp(cmd, "MP3_NEXT") == 0 || strcmp(cmd, "MNEXT") == 0) {
    g_mp3.nextTrack();
    printMp3Status("next");
    return true;
  }

  if (strcmp(cmd, "MP3_PREV") == 0 || strcmp(cmd, "MPREV") == 0) {
    g_mp3.previousTrack();
    printMp3Status("prev");
    return true;
  }

  if (strcmp(cmd, "MP3_RESTART") == 0 || strcmp(cmd, "MRESTART") == 0) {
    g_mp3.restartTrack();
    printMp3Status("restart");
    return true;
  }

  int trackNum = 0;
  if (sscanf(cmd, "MP3_PLAY %d", &trackNum) == 1 || sscanf(cmd, "MPLAY %d", &trackNum) == 1) {
    if (trackNum < 1) {
      Serial.println("[MP3_DBG] MP3_PLAY invalide: track>=1.");
      return true;
    }
    g_mp3.requestStorageRefresh();
    g_mp3.update(nowMs, g_mode == RuntimeMode::kMp3);
    const uint16_t count = g_mp3.trackCount();
    if (!g_mp3.isSdReady() || count == 0) {
      Serial.println("[MP3_DBG] MP3_PLAY refuse: SD/tracks indisponibles.");
      return true;
    }
    if (trackNum > static_cast<int>(count)) {
      Serial.printf("[MP3_DBG] MP3_PLAY refuse: track=%d > count=%u.\n",
                    trackNum,
                    static_cast<unsigned int>(count));
      return true;
    }

    if (!g_mp3.selectTrackByIndex(static_cast<uint16_t>(trackNum - 1), true)) {
      Serial.printf("[MP3_DBG] MP3_PLAY failed: idx=%d\n", trackNum - 1);
      return true;
    }
    printMp3Status("play");
    return true;
  }

  if (strcmp(cmd, "MP3_FX_STOP") == 0 || strcmp(cmd, "MFX STOP") == 0) {
    audioService().stopOverlay("serial_mp3_fx_stop");
    printMp3Status("fx_stop");
    return true;
  }

  char modeToken[16] = {};
  if (sscanf(cmd, "MP3_FX_MODE %15s", modeToken) == 1 || sscanf(cmd, "MFX MODE %15s", modeToken) == 1) {
    if (strcmp(modeToken, "DUCK") == 0 || strcmp(modeToken, "DUCKING") == 0) {
      g_mp3.setFxMode(Mp3FxMode::kDucking);
      Serial.println("[MP3_FX] mode=DUCKING");
      printMp3Status("fx_mode");
      return true;
    }
    if (strcmp(modeToken, "OVERLAY") == 0 || strcmp(modeToken, "MIX") == 0) {
      g_mp3.setFxMode(Mp3FxMode::kOverlay);
      Serial.println("[MP3_FX] mode=OVERLAY");
      printMp3Status("fx_mode");
      return true;
    }
    Serial.println("[MP3_FX] MP3_FX_MODE invalide: DUCK|OVERLAY.");
    return true;
  }

  int duckPct = -1;
  int mixPct = -1;
  if (sscanf(cmd, "MP3_FX_GAIN %d %d", &duckPct, &mixPct) == 2 ||
      sscanf(cmd, "MFX GAIN %d %d", &duckPct, &mixPct) == 2) {
    if (duckPct < 0 || duckPct > 100 || mixPct < 0 || mixPct > 100) {
      Serial.println("[MP3_FX] MP3_FX_GAIN invalide: 0..100 0..100.");
      return true;
    }
    g_mp3.setFxDuckingGain(static_cast<float>(duckPct) / 100.0f);
    g_mp3.setFxOverlayGain(static_cast<float>(mixPct) / 100.0f);
    printMp3Status("fx_gain");
    return true;
  }

  char fxToken[16] = {};
  int fxDurationMs = 0;
  if (sscanf(cmd, "MP3_FX %15s %d", fxToken, &fxDurationMs) >= 1 ||
      sscanf(cmd, "MFX %15s %d", fxToken, &fxDurationMs) >= 1) {
    Mp3FxEffect effect = Mp3FxEffect::kFmSweep;
    if (!parseMp3FxEffectToken(fxToken, &effect)) {
      Serial.println("[MP3_FX] MP3_FX invalide: FM|SONAR|MORSE|WIN [ms].");
      return true;
    }

    forceUsonFunctionalForMp3Debug("serial_mp3_fx");
    g_mp3.requestStorageRefresh();
    g_mp3.update(nowMs, g_mode == RuntimeMode::kMp3);
    triggerMp3Fx(effect,
                 (fxDurationMs > 0) ? static_cast<uint32_t>(fxDurationMs)
                                    : static_cast<uint32_t>(config::kMp3FxDefaultDurationMs),
                 "serial_mp3_fx");
    printMp3Status("fx");
    return true;
  }

  int dwellMs = 3500;
  if (sscanf(cmd, "MP3_TEST_START %d", &dwellMs) >= 1 ||
      sscanf(cmd, "MTEST START %d", &dwellMs) >= 1) {
    if (dwellMs < 1600) {
      dwellMs = 1600;
    } else if (dwellMs > 15000) {
      dwellMs = 15000;
    }

    forceUsonFunctionalForMp3Debug("serial_mp3_test");
    g_mp3.requestStorageRefresh();
    g_mp3.update(nowMs, false);
    if (!g_mp3.isSdReady() || g_mp3.trackCount() == 0) {
      Serial.println("[MP3_TEST] START refuse: SD/tracks indisponibles.");
      return true;
    }

    stopMp3FormatTest("restart");
    g_mp3FormatTest.active = true;
    g_mp3FormatTest.totalTracks = g_mp3.trackCount();
    g_mp3FormatTest.testedTracks = 0;
    g_mp3FormatTest.okTracks = 0;
    g_mp3FormatTest.failTracks = 0;
    g_mp3FormatTest.dwellMs = static_cast<uint32_t>(dwellMs);
    g_mp3FormatTest.stageStartMs = nowMs;
    g_mp3FormatTest.stageResultLogged = false;

    g_mp3.selectTrackByIndex(0U, true);

    Serial.printf("[MP3_TEST] START tracks=%u dwell=%lu ms\n",
                  static_cast<unsigned int>(g_mp3FormatTest.totalTracks),
                  static_cast<unsigned long>(g_mp3FormatTest.dwellMs));
    printMp3Status("test_start");
    return true;
  }

  if (strcmp(cmd, "MP3_TEST_STOP") == 0 || strcmp(cmd, "MTEST STOP") == 0) {
    stopMp3FormatTest("serial_stop");
    return true;
  }

  return false;
}

void updateMp3FormatTest(uint32_t nowMs) {
  if (!g_mp3FormatTest.active) {
    return;
  }

  if (!g_mp3.isSdReady() || g_mp3.trackCount() == 0) {
    stopMp3FormatTest("sd_unavailable");
    return;
  }

  const uint32_t elapsed = nowMs - g_mp3FormatTest.stageStartMs;
  if (!g_mp3FormatTest.stageResultLogged && elapsed >= 900U) {
    const bool ok = g_mp3.isPlaying();
    const String path = g_mp3.currentTrackName();
    const BootFsCodec codec = bootFsCodecFromPath(path.c_str());
    ++g_mp3FormatTest.testedTracks;
    if (ok) {
      ++g_mp3FormatTest.okTracks;
    } else {
      ++g_mp3FormatTest.failTracks;
    }
    Serial.printf("[MP3_TEST] #%u/%u play=%u codec=%s file=%s\n",
                  static_cast<unsigned int>(g_mp3FormatTest.testedTracks),
                  static_cast<unsigned int>(g_mp3FormatTest.totalTracks),
                  ok ? 1U : 0U,
                  bootFsCodecLabel(codec),
                  path.isEmpty() ? "-" : path.c_str());
    g_mp3FormatTest.stageResultLogged = true;
  }

  if (elapsed < g_mp3FormatTest.dwellMs) {
    return;
  }

  if (g_mp3FormatTest.testedTracks >= g_mp3FormatTest.totalTracks) {
    stopMp3FormatTest("done");
    return;
  }

  g_mp3.nextTrack();
  g_mp3FormatTest.stageStartMs = nowMs;
  g_mp3FormatTest.stageResultLogged = false;
}

void printStoryDebugHelp() {
  Serial.println("[STORY] Flow: UNLOCK -> WIN -> attente -> ETAPE_2 -> gate MP3 ouvert.");
  Serial.println("[STORY] Cmd: STORY_STATUS | STORY_RESET | STORY_ARM | STORY_FORCE_ETAPE2");
  Serial.println("[STORY] Cmd: STORY_TEST_ON | STORY_TEST_OFF | STORY_TEST_DELAY <ms>");
}

bool processStoryDebugCommand(const char* cmd, uint32_t nowMs) {
  if (strcmp(cmd, "STORY_STATUS") == 0 || strcmp(cmd, "SSTATUS") == 0) {
    storyController().printStatus(nowMs, "serial_story_status");
    return true;
  }
  if (strcmp(cmd, "STORY_HELP") == 0 || strcmp(cmd, "SHELP") == 0) {
    printStoryDebugHelp();
    return true;
  }
  if (strcmp(cmd, "STORY_RESET") == 0 || strcmp(cmd, "SRESET") == 0) {
    storyController().reset("serial_story_reset");
    return true;
  }
  if (strcmp(cmd, "STORY_ARM") == 0 || strcmp(cmd, "SARM") == 0) {
    armStoryTimelineAfterUnlock(nowMs);
    storyController().printStatus(nowMs, "serial_story_arm");
    return true;
  }
  if (strcmp(cmd, "STORY_FORCE_ETAPE2") == 0 || strcmp(cmd, "SETAPE2") == 0) {
    storyController().forceEtape2DueNow(nowMs, "serial_story_force");
    updateStoryTimeline(nowMs);
    storyController().printStatus(nowMs, "serial_story_force");
    return true;
  }
  if (strcmp(cmd, "STORY_TEST_ON") == 0 || strcmp(cmd, "STEST ON") == 0) {
    storyController().setTestMode(true, nowMs, "serial_story_test_on");
    return true;
  }
  if (strcmp(cmd, "STORY_TEST_OFF") == 0 || strcmp(cmd, "STEST OFF") == 0) {
    storyController().setTestMode(false, nowMs, "serial_story_test_off");
    return true;
  }

  uint32_t delayMs = 0U;
  if (sscanf(cmd, "STORY_TEST_DELAY %lu", &delayMs) == 1 ||
      sscanf(cmd, "STEST DELAY %lu", &delayMs) == 1) {
    storyController().setTestDelayMs(delayMs, nowMs, "serial_story_test_delay");
    return true;
  }

  return false;
}

void printKeyTuneHelp() {
  Serial.println("[KEY_TUNE] Cmd: KEY_STATUS | KEY_RAW_ON | KEY_RAW_OFF | KEY_RESET");
  Serial.println("[KEY_TUNE] Cmd: KEY_SET K4 1500 | KEY_SET K6 2200 | KEY_SET REL 3920");
  Serial.println("[KEY_TUNE] Cmd: KEY_SET_ALL k1 k2 k3 k4 k5 k6 rel");
  Serial.println("[KEY_TUNE] Cmd: KEY_TEST_START | KEY_TEST_STATUS | KEY_TEST_RESET | KEY_TEST_STOP");
  Serial.println("[KEY_TUNE] Cmd: BOOT_FS_INFO | BOOT_FS_LIST | BOOT_FS_TEST");
  Serial.println("[KEY_TUNE] Cmd: BOOT_FX_FM | BOOT_FX_SONAR | BOOT_FX_MORSE | BOOT_FX_WIN");
  Serial.println(
      "[KEY_TUNE] Cmd: STORY_STATUS | STORY_TEST_ON/OFF | STORY_TEST_DELAY | STORY_ARM | STORY_FORCE_ETAPE2");
  Serial.println("[KEY_TUNE] Cmd: CODEC_STATUS | CODEC_DUMP | CODEC_RD/WR | CODEC_VOL");
  Serial.println("[KEY_TUNE] Cmd: MP3_STATUS | MP3_UNLOCK | MP3_REFRESH | MP3_LIST | MP3_TEST_START | MP3_FX");
}

void processKeyTuneSerialCommand(const char* rawCmd, uint32_t nowMs) {
  if (rawCmd == nullptr || rawCmd[0] == '\0') {
    return;
  }

  char cmd[80] = {};
  size_t src = 0;
  while (rawCmd[src] != '\0' && isspace(static_cast<unsigned char>(rawCmd[src])) != 0) {
    ++src;
  }

  size_t dst = 0;
  while (rawCmd[src] != '\0' && dst < (sizeof(cmd) - 1U)) {
    cmd[dst++] = static_cast<char>(toupper(static_cast<unsigned char>(rawCmd[src++])));
  }
  while (dst > 0U && isspace(static_cast<unsigned char>(cmd[dst - 1U])) != 0) {
    --dst;
  }
  cmd[dst] = '\0';

  if (dst == 0U) {
    return;
  }

  const bool bootAlias = strcmp(cmd, "NEXT") == 0 || strcmp(cmd, "OK") == 0 ||
                         strcmp(cmd, "VALID") == 0 ||
                         strcmp(cmd, "REPLAY") == 0 || strcmp(cmd, "R") == 0 ||
                         strcmp(cmd, "KO") == 0 || strcmp(cmd, "NOK") == 0 ||
                         strcmp(cmd, "SKIP") == 0 || strcmp(cmd, "STATUS") == 0 ||
                         strcmp(cmd, "?") == 0 || strcmp(cmd, "HELP") == 0 ||
                         strcmp(cmd, "H") == 0 || strcmp(cmd, "TONE") == 0 ||
                         strcmp(cmd, "DIAG") == 0 || strcmp(cmd, "PA") == 0 ||
                         strcmp(cmd, "PAON") == 0 || strcmp(cmd, "PAOFF") == 0 ||
                         strcmp(cmd, "PAINV") == 0 || strcmp(cmd, "FS_INFO") == 0 ||
                         strcmp(cmd, "FS_LIST") == 0 || strcmp(cmd, "FSTEST") == 0 ||
                         strcmp(cmd, "FX_FM") == 0 || strcmp(cmd, "FX_SONAR") == 0 ||
                         strcmp(cmd, "FX_MORSE") == 0 || strcmp(cmd, "FX_WIN") == 0;
  if (strncmp(cmd, "BOOT_", 5U) == 0 || bootAlias) {
    processBootAudioSerialCommand(cmd, nowMs);
    return;
  }

  if (processCodecDebugCommand(cmd)) {
    return;
  }
  if (processStoryDebugCommand(cmd, nowMs)) {
    return;
  }
  if (processMp3DebugCommand(cmd, nowMs)) {
    return;
  }

  if (strcmp(cmd, "KEY_HELP") == 0 || strcmp(cmd, "KHELP") == 0 || strcmp(cmd, "KEY") == 0) {
    printKeyTuneHelp();
    return;
  }
  if (strcmp(cmd, "KEY_STATUS") == 0 || strcmp(cmd, "KSTAT") == 0) {
    printKeyTuneThresholds("status");
    Serial.printf("[KEY_TUNE] raw=%u stable=K%u\n",
                  static_cast<unsigned int>(g_keypad.lastRaw()),
                  static_cast<unsigned int>(g_keypad.currentKey()));
    printKeySelfTestStatus("status");
    return;
  }
  if (strcmp(cmd, "KEY_TEST_START") == 0 || strcmp(cmd, "KTEST START") == 0) {
    startKeySelfTest();
    return;
  }
  if (strcmp(cmd, "KEY_TEST_STATUS") == 0 || strcmp(cmd, "KTEST STATUS") == 0) {
    printKeySelfTestStatus("status");
    return;
  }
  if (strcmp(cmd, "KEY_TEST_RESET") == 0 || strcmp(cmd, "KTEST RESET") == 0) {
    resetKeySelfTestStats();
    g_keySelfTest.active = true;
    printKeySelfTestStatus("reset");
    return;
  }
  if (strcmp(cmd, "KEY_TEST_STOP") == 0 || strcmp(cmd, "KTEST STOP") == 0) {
    stopKeySelfTest("stop");
    return;
  }
  if (strcmp(cmd, "KEY_RAW_ON") == 0 || strcmp(cmd, "KRAW ON") == 0) {
    g_keyTune.rawStreamEnabled = true;
    g_keyTune.nextRawLogMs = nowMs;
    Serial.println("[KEY_TUNE] raw stream ON");
    return;
  }
  if (strcmp(cmd, "KEY_RAW_OFF") == 0 || strcmp(cmd, "KRAW OFF") == 0) {
    g_keyTune.rawStreamEnabled = false;
    Serial.println("[KEY_TUNE] raw stream OFF");
    return;
  }
  if (strcmp(cmd, "KEY_RESET") == 0) {
    g_keypad.resetThresholdsToDefault();
    printKeyTuneThresholds("reset_defaults");
    return;
  }

  int k1 = 0;
  int k2 = 0;
  int k3 = 0;
  int k4 = 0;
  int k5 = 0;
  int k6 = 0;
  int rel = 0;
  if (sscanf(cmd, "KEY_SET_ALL %d %d %d %d %d %d %d", &k1, &k2, &k3, &k4, &k5, &k6, &rel) == 7) {
    if (k1 < 0 || k2 < 0 || k3 < 0 || k4 < 0 || k5 < 0 || k6 < 0 || rel < 0 || k1 > 4095 ||
        k2 > 4095 || k3 > 4095 || k4 > 4095 || k5 > 4095 || k6 > 4095 || rel > 4095) {
      Serial.println("[KEY_TUNE] KEY_SET_ALL invalide: bornes 0..4095.");
      return;
    }

    KeypadAnalog::Thresholds thresholds;
    thresholds.keyMax[0] = static_cast<uint16_t>(k1);
    thresholds.keyMax[1] = static_cast<uint16_t>(k2);
    thresholds.keyMax[2] = static_cast<uint16_t>(k3);
    thresholds.keyMax[3] = static_cast<uint16_t>(k4);
    thresholds.keyMax[4] = static_cast<uint16_t>(k5);
    thresholds.keyMax[5] = static_cast<uint16_t>(k6);
    thresholds.releaseThreshold = static_cast<uint16_t>(rel);
    if (!g_keypad.setThresholds(thresholds)) {
      Serial.println("[KEY_TUNE] KEY_SET_ALL refuse: ordre strict requis et REL > K6.");
      return;
    }
    printKeyTuneThresholds("set_all");
    return;
  }

  char selector[16] = {};
  int value = 0;
  if (sscanf(cmd, "KEY_SET %15s %d", selector, &value) == 2) {
    if (value < 0 || value > 4095) {
      Serial.println("[KEY_TUNE] KEY_SET invalide: valeur 0..4095.");
      return;
    }

    const uint16_t rawMax = static_cast<uint16_t>(value);
    if (strcmp(selector, "REL") == 0 || strcmp(selector, "RELEASE") == 0 ||
        strcmp(selector, "R") == 0) {
      if (!g_keypad.setReleaseThreshold(rawMax)) {
        Serial.println("[KEY_TUNE] KEY_SET REL refuse: REL doit etre > K6.");
        return;
      }
      printKeyTuneThresholds("set_rel");
      return;
    }

    uint8_t keyIndex = 0;
    if (selector[0] == 'K' && selector[1] >= '1' && selector[1] <= '6' && selector[2] == '\0') {
      keyIndex = static_cast<uint8_t>(selector[1] - '0');
    } else if (selector[0] >= '1' && selector[0] <= '6' && selector[1] == '\0') {
      keyIndex = static_cast<uint8_t>(selector[0] - '0');
    }

    if (keyIndex == 0) {
      Serial.println("[KEY_TUNE] KEY_SET invalide: utiliser K1..K6 ou REL.");
      return;
    }

    if (!g_keypad.setKeyMax(keyIndex, rawMax)) {
      Serial.println("[KEY_TUNE] KEY_SET refuse: verifier ordre K1<K2<...<K6<REL.");
      return;
    }
    printKeyTuneThresholds("set_key");
    return;
  }

  Serial.printf("[KEY_TUNE] Commande inconnue: %s\n", cmd);
}

void pollKeyTuneSerial(uint32_t nowMs) {
  if (g_bootAudioProtocol.active) {
    return;
  }

  while (Serial.available() > 0) {
    const char c = static_cast<char>(Serial.read());
    if (c == '\r') {
      continue;
    }

    if (c == '\n') {
      g_keyTune.serialCmdBuffer[g_keyTune.serialCmdLen] = '\0';
      processKeyTuneSerialCommand(g_keyTune.serialCmdBuffer, nowMs);
      g_keyTune.serialCmdLen = 0;
      continue;
    }

    if (g_keyTune.serialCmdLen < (sizeof(g_keyTune.serialCmdBuffer) - 1U)) {
      g_keyTune.serialCmdBuffer[g_keyTune.serialCmdLen++] = c;
    } else {
      g_keyTune.serialCmdLen = 0;
    }
  }
}

bool commandMatches(const char* cmd, const char* token) {
  if (cmd == nullptr || token == nullptr) {
    return false;
  }
  const size_t tokenLen = strlen(token);
  if (strncmp(cmd, token, tokenLen) != 0) {
    return false;
  }
  return cmd[tokenLen] == '\0' || cmd[tokenLen] == ' ';
}

bool isCanonicalSerialCommand(const char* cmd) {
  static const char* kCanonicalCommands[] = {
      "BOOT_STATUS", "BOOT_HELP",   "BOOT_NEXT",       "BOOT_REPLAY", "BOOT_REOPEN",
      "BOOT_TEST_TONE",             "BOOT_TEST_DIAG",  "BOOT_PA_ON",  "BOOT_PA_OFF",
      "BOOT_PA_STATUS",             "BOOT_PA_INV",     "BOOT_FS_INFO","BOOT_FS_LIST",
      "BOOT_FS_TEST",               "BOOT_FX_FM",      "BOOT_FX_SONAR","BOOT_FX_MORSE",
      "BOOT_FX_WIN",                "STORY_STATUS",    "STORY_HELP",  "STORY_RESET",
      "STORY_ARM",                  "STORY_FORCE_ETAPE2", "STORY_TEST_ON",
      "STORY_TEST_OFF",             "STORY_TEST_DELAY","MP3_HELP",    "MP3_STATUS",
      "MP3_UNLOCK",
      "MP3_REFRESH",                "MP3_LIST",        "MP3_NEXT",    "MP3_PREV",
      "MP3_RESTART",                "MP3_PLAY",        "MP3_FX_MODE", "MP3_FX_GAIN",
      "MP3_FX",                     "MP3_FX_STOP",     "MP3_TEST_START", "MP3_TEST_STOP",
      "MP3_BACKEND",                "MP3_SCAN",        "MP3_BROWSE",  "MP3_PLAY_PATH",
      "MP3_UI",                     "MP3_STATE",
      "KEY_HELP",                   "KEY_STATUS",      "KEY_RAW_ON",  "KEY_RAW_OFF",
      "KEY_RESET",                  "KEY_SET",         "KEY_SET_ALL", "KEY_TEST_START",
      "KEY_TEST_STATUS",            "KEY_TEST_RESET",  "KEY_TEST_STOP", "CODEC_HELP",
      "CODEC_STATUS",               "CODEC_DUMP",      "CODEC_RD",    "CODEC_WR",
      "CODEC_VOL",                  "CODEC_VOL_RAW",
  };

  for (const char* token : kCanonicalCommands) {
    if (commandMatches(cmd, token)) {
      return true;
    }
  }
  return false;
}

void onSerialCommand(const char* cmd, uint32_t nowMs, void* ctx) {
  (void)ctx;
  if (cmd == nullptr || cmd[0] == '\0') {
    return;
  }
  if (!isCanonicalSerialCommand(cmd)) {
    Serial.printf("[SERIAL] UNKNOWN (canonique attendu): %s\n", cmd);
    return;
  }

  if (strncmp(cmd, "BOOT_", 5U) == 0) {
    processBootAudioSerialCommand(cmd, nowMs);
    return;
  }
  if (strncmp(cmd, "STORY_", 6U) == 0) {
    if (!processStoryDebugCommand(cmd, nowMs)) {
      Serial.printf("[SERIAL] UNKNOWN STORY: %s\n", cmd);
    }
    return;
  }
  if (strncmp(cmd, "MP3_", 4U) == 0) {
    if (!processMp3DebugCommand(cmd, nowMs)) {
      Serial.printf("[SERIAL] UNKNOWN MP3: %s\n", cmd);
    }
    return;
  }
  if (strncmp(cmd, "KEY_", 4U) == 0) {
    processKeyTuneSerialCommand(cmd, nowMs);
    return;
  }
  if (strncmp(cmd, "CODEC_", 6U) == 0) {
    if (!processCodecDebugCommand(cmd)) {
      Serial.printf("[SERIAL] UNKNOWN CODEC: %s\n", cmd);
    }
    return;
  }

  Serial.printf("[SERIAL] UNKNOWN PREFIX: %s\n", cmd);
}

void updateKeyTuneRawStream(uint32_t nowMs) {
  if (!g_keyTune.rawStreamEnabled) {
    return;
  }
  if (static_cast<int32_t>(nowMs - g_keyTune.nextRawLogMs) < 0) {
    return;
  }

  g_keyTune.nextRawLogMs = nowMs + 120U;
  Serial.printf("[KEY_RAW] raw=%u stable=K%u\n",
                static_cast<unsigned int>(g_keypad.lastRaw()),
                static_cast<unsigned int>(g_keypad.currentKey()));
}

void resetMicCalibrationStats() {
  g_micCalibration.samples = 0;
  g_micCalibration.rmsMin = 1000000.0f;
  g_micCalibration.rmsMax = 0.0f;
  g_micCalibration.ratioMin = 1000000.0f;
  g_micCalibration.ratioMax = 0.0f;
  g_micCalibration.p2pMin = 0xFFFF;
  g_micCalibration.p2pMax = 0;
  g_micCalibration.okCount = 0;
  g_micCalibration.silenceCount = 0;
  g_micCalibration.saturationCount = 0;
  g_micCalibration.tooLoudCount = 0;
  g_micCalibration.detectOffCount = 0;
}

void startMicCalibration(uint32_t nowMs, const char* reason) {
  g_micCalibration.active = true;
  g_micCalibration.untilMs = nowMs + config::kMicCalibrationDurationMs;
  g_micCalibration.nextLogMs = nowMs;
  resetMicCalibrationStats();
  Serial.printf("[MIC_CAL] START reason=%s duration=%lu ms\n",
                reason,
                static_cast<unsigned long>(config::kMicCalibrationDurationMs));
}

void stopMicCalibration(uint32_t nowMs, const char* reason) {
  if (!g_micCalibration.active) {
    return;
  }

  g_micCalibration.active = false;
  Serial.printf("[MIC_CAL] STOP reason=%s now=%lu ms\n",
                reason,
                static_cast<unsigned long>(nowMs));

  if (g_micCalibration.samples == 0) {
    Serial.println("[MIC_CAL] SUMMARY no sample captured.");
    return;
  }

  Serial.printf(
      "[MIC_CAL] SUMMARY n=%lu rms[min/max]=%.1f/%.1f p2p[min/max]=%u/%u ratio[min/max]=%.3f/%.3f\n",
      static_cast<unsigned long>(g_micCalibration.samples),
      static_cast<double>(g_micCalibration.rmsMin),
      static_cast<double>(g_micCalibration.rmsMax),
      static_cast<unsigned int>(g_micCalibration.p2pMin),
      static_cast<unsigned int>(g_micCalibration.p2pMax),
      static_cast<double>(g_micCalibration.ratioMin),
      static_cast<double>(g_micCalibration.ratioMax));
  Serial.printf("[MIC_CAL] HEALTH ok=%u silence=%u saturation=%u too_loud=%u detect_off=%u\n",
                static_cast<unsigned int>(g_micCalibration.okCount),
                static_cast<unsigned int>(g_micCalibration.silenceCount),
                static_cast<unsigned int>(g_micCalibration.saturationCount),
                static_cast<unsigned int>(g_micCalibration.tooLoudCount),
                static_cast<unsigned int>(g_micCalibration.detectOffCount));

  if (g_micCalibration.saturationCount > 0) {
    Serial.println("[MIC_CAL] DIAG saturation detectee (niveau trop fort ou biais incorrect).");
  } else if (g_micCalibration.silenceCount > (g_micCalibration.samples / 2U)) {
    Serial.println("[MIC_CAL] DIAG signal faible: verifier micro, cablage ou gain.");
  } else if (g_micCalibration.okCount > (g_micCalibration.samples / 2U)) {
    Serial.println("[MIC_CAL] DIAG micro globalement OK.");
  } else {
    Serial.println("[MIC_CAL] DIAG etat mixte: verifier position/gain/source audio.");
  }
}

void updateMicCalibration(uint32_t nowMs,
                          bool laDetected,
                          int8_t tuningOffset,
                          uint8_t tuningConfidence,
                          float ratio,
                          float mean,
                          float rms,
                          uint16_t micMin,
                          uint16_t micMax,
                          const char* healthLabel) {
  if (!g_micCalibration.active) {
    return;
  }

  if (static_cast<int32_t>(nowMs - g_micCalibration.nextLogMs) < 0) {
    if (static_cast<int32_t>(nowMs - g_micCalibration.untilMs) >= 0) {
      stopMicCalibration(nowMs, "timeout");
    }
    return;
  }
  g_micCalibration.nextLogMs = nowMs + config::kMicCalibrationLogPeriodMs;

  const uint16_t p2p = static_cast<uint16_t>(micMax - micMin);
  ++g_micCalibration.samples;
  if (rms < g_micCalibration.rmsMin) {
    g_micCalibration.rmsMin = rms;
  }
  if (rms > g_micCalibration.rmsMax) {
    g_micCalibration.rmsMax = rms;
  }
  if (ratio < g_micCalibration.ratioMin) {
    g_micCalibration.ratioMin = ratio;
  }
  if (ratio > g_micCalibration.ratioMax) {
    g_micCalibration.ratioMax = ratio;
  }
  if (p2p < g_micCalibration.p2pMin) {
    g_micCalibration.p2pMin = p2p;
  }
  if (p2p > g_micCalibration.p2pMax) {
    g_micCalibration.p2pMax = p2p;
  }

  if (strcmp(healthLabel, "OK") == 0) {
    ++g_micCalibration.okCount;
  } else if (strcmp(healthLabel, "SILENCE/GAIN") == 0) {
    ++g_micCalibration.silenceCount;
  } else if (strcmp(healthLabel, "SATURATION") == 0) {
    ++g_micCalibration.saturationCount;
  } else if (strcmp(healthLabel, "TOO_LOUD") == 0) {
    ++g_micCalibration.tooLoudCount;
  } else if (strcmp(healthLabel, "DETECT_OFF") == 0) {
    ++g_micCalibration.detectOffCount;
  }

  const uint32_t leftMs =
      (static_cast<int32_t>(g_micCalibration.untilMs - nowMs) > 0)
          ? (g_micCalibration.untilMs - nowMs)
          : 0;
  Serial.printf(
      "[MIC_CAL] left=%lus det=%u off=%d conf=%u ratio=%.3f mean=%.1f rms=%.1f min=%u max=%u p2p=%u health=%s\n",
      static_cast<unsigned long>(leftMs / 1000UL),
      laDetected ? 1U : 0U,
      static_cast<int>(tuningOffset),
      static_cast<unsigned int>(tuningConfidence),
      static_cast<double>(ratio),
      static_cast<double>(mean),
      static_cast<double>(rms),
      static_cast<unsigned int>(micMin),
      static_cast<unsigned int>(micMax),
      static_cast<unsigned int>(p2p),
      healthLabel);

  if (static_cast<int32_t>(nowMs - g_micCalibration.untilMs) >= 0) {
    stopMicCalibration(nowMs, "timeout");
  }
}

AppSchedulerInputs makeSchedulerInputs() {
  AppSchedulerInputs input;
  input.currentMode = g_mode;
  input.uSonFunctional = g_uSonFunctional;
  input.unlockJingleActive = g_unlockJingle.active;
  input.sdReady = g_mp3.isSdReady();
  input.hasTracks = g_mp3.hasTracks();
  input.mp3GateOpen = isMp3GateOpen();
  input.laDetectionEnabled = g_laDetectionEnabled;
  input.sineEnabled = config::kEnableSineDac;
  input.bootProtocolActive = g_bootAudioProtocol.active;
  return input;
}

void applyRuntimeMode(RuntimeMode newMode, bool force = false) {
  const bool changed = (newMode != g_mode);
  if (!changed && !force) {
    return;
  }

  g_mode = newMode;
  if (g_mode == RuntimeMode::kMp3) {
    stopUnlockJingle(false);
    stopMicCalibration(millis(), "mode_mp3");
    cancelULockSearchSonarCue("mode_mp3");
    g_laDetectionEnabled = false;
    g_laDetector.setCaptureEnabled(false);
    g_sine.setEnabled(false);
    if (changed) {
      Serial.println("[MODE] LECTEUR U-SON (SD detectee)");
    }
  } else {
    stopUnlockJingle(false);
    g_uSonFunctional = false;
    cancelULockSearchSonarCue("mode_signal");
    g_uLockListening = !config::kULockRequireKeyToStartDetection;
    resetStoryTimeline(changed ? "mode_signal" : "boot_signal");
    resetLaHoldProgress();
    g_laDetectionEnabled = g_uLockListening;
    g_laDetector.setCaptureEnabled(g_uLockListening);
    if (config::kEnableMicCalibrationOnSignalEntry && g_uLockListening) {
      startMicCalibration(millis(), changed ? "mode_signal" : "boot_signal");
    } else {
      stopMicCalibration(millis(), "ulock_wait_key");
    }
    if (config::kEnableSineDac) {
      g_sine.setEnabled(true);
    }
    if (changed) {
      Serial.println("[MODE] U_LOCK (appuyer touche pour detecter LA)");
    }
  }
}

void handleKeyPress(uint8_t key) {
  if (g_mode == RuntimeMode::kMp3) {
    g_playerUi.setBrowserBounds(g_mp3.trackCount());
    const PlayerUiPage page = currentPlayerUiPage();

    switch (key) {
      case 1:
        if (page == PlayerUiPage::kBrowser) {
          if (g_mp3.selectTrackByIndex(g_playerUi.cursor(), true)) {
            Serial.printf("[KEY] K1 SELECT %u/%u\n",
                          static_cast<unsigned int>(g_mp3.currentTrackNumber()),
                          static_cast<unsigned int>(g_mp3.trackCount()));
          } else {
            Serial.printf("[KEY] K1 SELECT refuse idx=%u\n",
                          static_cast<unsigned int>(g_playerUi.cursor()));
          }
        } else if (page == PlayerUiPage::kSettings) {
          g_mp3.cycleRepeatMode();
          Serial.printf("[KEY] K1 REPEAT %s\n", g_mp3.repeatModeLabel());
        } else {
          g_mp3.togglePause();
          Serial.printf("[KEY] K1 MP3 %s\n", g_mp3.isPaused() ? "PAUSE" : "PLAY");
        }
        break;
      case 2:
        if (page == PlayerUiPage::kNowPlaying) {
          g_mp3.previousTrack();
          Serial.printf("[KEY] K2 PREV %u/%u\n",
                        static_cast<unsigned int>(g_mp3.currentTrackNumber()),
                        static_cast<unsigned int>(g_mp3.trackCount()));
        } else {
          UiAction action;
          action.source = UiActionSource::kKeyShort;
          action.key = 2U;
          g_playerUi.applyAction(action);
          Serial.printf("[KEY] K2 CURSOR %u\n", static_cast<unsigned int>(g_playerUi.cursor()));
        }
        break;
      case 3:
        if (page == PlayerUiPage::kNowPlaying) {
          g_mp3.nextTrack();
          Serial.printf("[KEY] K3 NEXT %u/%u\n",
                        static_cast<unsigned int>(g_mp3.currentTrackNumber()),
                        static_cast<unsigned int>(g_mp3.trackCount()));
        } else {
          UiAction action;
          action.source = UiActionSource::kKeyShort;
          action.key = 3U;
          g_playerUi.applyAction(action);
          Serial.printf("[KEY] K3 CURSOR %u\n", static_cast<unsigned int>(g_playerUi.cursor()));
        }
        break;
      case 4:
        g_mp3.setGain(g_mp3.gain() - 0.05f);
        Serial.printf("[KEY] K4 VOL- %u%%\n", static_cast<unsigned int>(g_mp3.volumePercent()));
        break;
      case 5:
        g_mp3.setGain(g_mp3.gain() + 0.05f);
        Serial.printf("[KEY] K5 VOL+ %u%%\n", static_cast<unsigned int>(g_mp3.volumePercent()));
        break;
      case 6:
        {
          UiAction action;
          action.source = UiActionSource::kKeyShort;
          action.key = 6U;
          g_playerUi.applyAction(action);
          Serial.printf("[KEY] K6 PAGE %s\n", playerUiPageLabel(g_playerUi.page()));
        }
        break;
      default:
        break;
    }
    return;
  }

  if (!g_uSonFunctional) {
    if (!g_uLockListening) {
      g_uLockListening = true;
      resetLaHoldProgress();
      g_laDetectionEnabled = true;
      g_laDetector.setCaptureEnabled(true);
      if (config::kEnableMicCalibrationOnSignalEntry) {
        startMicCalibration(millis(), "key_start_ulock_detect");
      }
      requestULockSearchSonarCue("key_start_ulock_detect");
      Serial.printf("[MODE] U_LOCK -> detection LA activee (K%u)\n",
                    static_cast<unsigned int>(key));
      return;
    }

    if (key == 6) {
      startMicCalibration(millis(), "key_k6_ulock");
      Serial.println("[KEY] K6 calibration micro (U_LOCK)");
      return;
    }
    Serial.printf("[KEY] K%u ignoree (U_LOCK detect en cours)\n", static_cast<unsigned int>(key));
    return;
  }

  switch (key) {
    case 1:
      g_laDetectionEnabled = !g_laDetectionEnabled;
      Serial.printf("[KEY] K1 LA DETECT %s\n", g_laDetectionEnabled ? "ON" : "OFF");
      break;
    case 2:
      Serial.println("[KEY] K2 I2S FM sweep (async).");
      audioService().startBaseFx(AudioEffectId::kFmSweep, 0.30f, 900U, "key_k2_i2s_fx");
      break;
    case 3:
      Serial.println("[KEY] K3 I2S sonar (async).");
      audioService().startBaseFx(AudioEffectId::kSonar, 0.28f, 1300U, "key_k3_i2s_fx");
      break;
    case 4:
      Serial.println("[KEY] K4 I2S boot FX replay.");
      startBootAudioPrimaryFxAsync("key_k4_replay");
      break;
    case 5:
      g_mp3.requestStorageRefresh();
      Serial.println("[KEY] K5 SD refresh request");
      break;
    case 6:
      startMicCalibration(millis(), "key_k6_signal");
      Serial.println("[KEY] K6 calibration micro (30s)");
      break;
    default:
      break;
  }
}

void App::setup() {
  Serial.begin(115200);
  delay(200);

  g_led.begin();
  g_laDetector.begin();
  inputService().begin();
  if (config::kUseI2SMicInput) {
    randomSeed(static_cast<uint32_t>(micros()));
  } else {
    randomSeed(analogRead(config::kPinMicAdc));
  }
  g_sine.begin();
  if (!config::kEnableSineDac) {
    Serial.println("[SINE] Mode I2S-only: DAC desactive.");
  } else if (!g_sine.isAvailable()) {
    Serial.printf("[SINE] Profil actuel: pin=%u non-DAC, sine analogique indisponible.\n",
                  static_cast<unsigned int>(config::kPinDacSine));
  }
  setupInternalLittleFs();
  g_mp3.begin();
  g_mp3.setFxMode(config::kMp3FxOverlayModeDefault ? Mp3FxMode::kOverlay : Mp3FxMode::kDucking);
  g_mp3.setFxDuckingGain(config::kMp3FxDuckingGainDefault);
  g_mp3.setFxOverlayGain(config::kMp3FxOverlayGainDefault);
  g_playerUi.reset();
  g_mp3BrowsePath = "/";
  g_screen.begin();
  sendScreenFrameSnapshot(millis(), 0U);
  g_paEnableActiveHigh = config::kPinAudioPaEnableActiveHigh;
  if (config::kBootAudioPaTogglePulse && config::kPinAudioPaEnable >= 0) {
    setBootAudioPaEnabled(false, "boot_pa_pulse_off");
    delay(config::kBootAudioPaToggleMs);
  }
  setBootAudioPaEnabled(true, "boot_setup");
  printBootAudioOutputInfo("boot_setup");
  g_sine.setEnabled(false);
  applyRuntimeMode(schedulerSelectRuntimeMode(makeSchedulerInputs()), true);
  serialRouter().setDispatcher(onSerialCommand, nullptr);
  bootProtocolController().start(millis());

  Serial.println("[BOOT] U-SON / ESP32 Audio Kit A252 pret.");
  if (config::kDisableBoardRgbLeds) {
    Serial.println("[LED] RGB carte force OFF.");
  }
  Serial.printf("[MIC] Source: %s\n",
                config::kUseI2SMicInput ? "I2S codec onboard (DIN GPIO35)" : "ADC externe GPIO34");
  Serial.println("[KEYMAP][MP3] K1 play/pause, K2 prev, K3 next, K4 vol-, K5 vol+, K6 repeat");
  Serial.println("[BOOT] Boucle attente: random '*boot*' puis scan radio I2S 10..40s.");
  Serial.println("[BOOT] Appui touche pendant attente: lancement U_LOCK ecoute (detection LA).");
  Serial.println("[BOOT] Puis MODULE U-SON Fonctionnel apres detection LA.");
  Serial.println("[STORY] Fin U_LOCK: lecture random '*WIN*' (fallback effet synth WIN).");
  Serial.println("[STORY] Fin U-SON: lecture random '*ETAPE_2*' a T+15min apres unlock.");
  Serial.println("[BOOT] En U_LOCK: detection SD desactivee jusqu'au mode U-SON Fonctionnel.");
  if (config::kEnableBootAudioValidationProtocol) {
    Serial.println("[KEYMAP][BOOT_PROTO] K1..K6=NEXT | Serial: BOOT_NEXT, BOOT_REPLAY, BOOT_REOPEN");
    Serial.println("[KEYMAP][BOOT_PROTO] FX: BOOT_FX_FM | BOOT_FX_SONAR | BOOT_FX_MORSE | BOOT_FX_WIN");
  }
  Serial.println(
      "[KEY_TUNE] Serial: KEY_STATUS | KEY_RAW_ON/OFF | KEY_SET Kx/REL v | KEY_TEST_START/STATUS/RESET/STOP");
  Serial.println("[KEY_TUNE] Serial: BOOT_FX_FM | BOOT_FX_SONAR | BOOT_FX_MORSE | BOOT_FX_WIN");
  Serial.println(
      "[MP3_DBG] Serial: MP3_STATUS | MP3_UNLOCK | MP3_REFRESH | MP3_LIST | MP3_PLAY n | MP3_TEST_START [ms]");
  Serial.println("[MP3_DBG] Serial: MP3_FX_MODE DUCK|OVERLAY | MP3_FX_GAIN duck mix | MP3_FX FM|SONAR|MORSE|WIN [ms]");
  Serial.println(
      "[MP3_DBG] Serial: MP3_BACKEND STATUS|SET AUTO|AUDIO_TOOLS|LEGACY | MP3_SCAN START|STATUS|CANCEL|REBUILD");
  Serial.println(
      "[MP3_DBG] Serial: MP3_BROWSE LS [path] | MP3_BROWSE CD <path> | MP3_PLAY_PATH <path> | MP3_UI PAGE ... | MP3_STATE SAVE|LOAD|RESET");
  Serial.println("[FS] Serial: BOOT_FS_INFO | BOOT_FS_LIST | BOOT_FS_TEST");
  Serial.printf("[FS] Boot FX path: %s (%s)\n",
                config::kBootFxLittleFsPath,
                config::kPreferLittleFsBootFx ? "preferred" : "disabled");
  Serial.printf("[MP3_FX] default mode=%s duck=%u%% mix=%u%% dur=%u ms\n",
                g_mp3.fxModeLabel(),
                static_cast<unsigned int>(g_mp3.fxDuckingGain() * 100.0f),
                static_cast<unsigned int>(g_mp3.fxOverlayGain() * 100.0f),
                static_cast<unsigned int>(config::kMp3FxDefaultDurationMs));
  Serial.println("[KEYMAP][SIGNAL] actifs seulement apres unlock: K1 LA on/off, K2 tone 440 I2S, K3 diag I2S, K4 replay FX I2S, K5 refresh SD, K6 cal micro");
}

void App::loop() {
  uint32_t nowMs = millis();
  updateAsyncAudioService(nowMs);
  nowMs = millis();
  updateStoryTimeline(nowMs);
  serialRouter().update(nowMs);
  nowMs = millis();
  AppSchedulerInputs schedulerInput = makeSchedulerInputs();
  AppBrickSchedule schedule = schedulerBuildBricks(schedulerInput);

  if (schedule.runBootProtocol) {
    bootProtocolController().update(nowMs);
    nowMs = millis();
  }

  schedulerInput = makeSchedulerInputs();
  schedule = schedulerBuildBricks(schedulerInput);

  if (schedule.runUnlockJingle) {
    updateUnlockJingle(nowMs);
  }

  if (schedule.runMp3Service) {
    g_mp3.update(nowMs, schedule.allowMp3Playback);
    g_playerUi.setBrowserBounds(g_mp3.trackCount());
    nowMs = millis();
  }
  applyRuntimeMode(schedulerSelectRuntimeMode(makeSchedulerInputs()));
  updateMp3FormatTest(nowMs);
  nowMs = millis();

  const AppBrickSchedule postModeSchedule = schedulerBuildBricks(makeSchedulerInputs());
  if (postModeSchedule.runSineDac) {
    g_sine.update();
  }
  if (postModeSchedule.runLaDetector) {
    g_laDetector.update(nowMs);
  }
  inputService().update(nowMs);

  static uint8_t screenKey = 0;
  static uint32_t screenKeyUntilMs = 0;
  KeyEvent keyEvent;
  if (inputService().consumePress(&keyEvent)) {
    Serial.printf("[KEY] K%u raw=%u\n",
                  static_cast<unsigned int>(keyEvent.key),
                  static_cast<unsigned int>(keyEvent.raw));
    if (g_bootAudioProtocol.active) {
      bootProtocolController().onKey(keyEvent.key, nowMs);
    } else if (g_keySelfTest.active) {
      handleKeySelfTestPress(keyEvent.key, keyEvent.raw);
    } else {
      handleKeyPress(keyEvent.key);
    }
    nowMs = millis();
    screenKey = keyEvent.key;
    screenKeyUntilMs = nowMs + 1200;
  }
  if (screenKey != 0 && static_cast<int32_t>(nowMs - screenKeyUntilMs) >= 0) {
    screenKey = 0;
  }
  serviceULockSearchSonarCue(nowMs);
  updateKeyTuneRawStream(nowMs);

  const bool laDetected =
      (g_mode == RuntimeMode::kSignal) && g_laDetectionEnabled && g_laDetector.isDetected();
  const bool uLockModeBeforeUnlock = (g_mode == RuntimeMode::kSignal) && !g_uSonFunctional;
  const bool uLockListeningBeforeUnlock = uLockModeBeforeUnlock && g_uLockListening;
  uint32_t loopDeltaMs = 0;
  if (g_lastLoopMs != 0) {
    loopDeltaMs = nowMs - g_lastLoopMs;
    if (loopDeltaMs > 250U) {
      loopDeltaMs = 250U;
    }
  }
  g_lastLoopMs = nowMs;

  if (!uLockListeningBeforeUnlock) {
    resetLaHoldProgress();
  } else if (laDetected) {
    uint32_t nextHoldMs = g_laHoldAccumMs + loopDeltaMs;
    if (nextHoldMs > config::kLaUnlockHoldMs) {
      nextHoldMs = config::kLaUnlockHoldMs;
    }
    g_laHoldAccumMs = nextHoldMs;
  }

  const uint8_t laHoldPercentBeforeUnlock = unlockHoldPercent(g_laHoldAccumMs, uLockListeningBeforeUnlock);

  if (uLockListeningBeforeUnlock && g_laHoldAccumMs >= config::kLaUnlockHoldMs) {
    g_uSonFunctional = true;
    cancelULockSearchSonarCue("unlock");
    resetLaHoldProgress();
    armStoryTimelineAfterUnlock(nowMs);
    g_mp3.requestStorageRefresh();
    Serial.println("[MODE] MODULE U-SON Fonctionnel (LA detecte)");
    Serial.println("[SD] Detection SD activee.");
  }

  const bool uLockMode = (g_mode == RuntimeMode::kSignal) && !g_uSonFunctional;
  const bool uLockListening = uLockMode && g_uLockListening;
  const int8_t tuningOffset = uLockListening ? g_laDetector.tuningOffset() : 0;
  const uint8_t tuningConfidence = uLockListening ? g_laDetector.tuningConfidence() : 0;
  const float micRms = g_laDetector.micRms();
  const uint8_t micLevelPercent = micLevelPercentFromRms(micRms);
  const uint16_t micMin = g_laDetector.micMin();
  const uint16_t micMax = g_laDetector.micMax();
  const uint16_t micP2P = g_laDetector.micPeakToPeak();
  const float targetRatio = g_laDetector.targetRatio();
  const float micMean = g_laDetector.micMean();
  const char* micHealth = micHealthLabel(g_laDetectionEnabled, micRms, micMin, micMax);

  if (g_mode == RuntimeMode::kSignal) {
    updateMicCalibration(nowMs,
                         laDetected,
                         tuningOffset,
                         tuningConfidence,
                         targetRatio,
                         micMean,
                         micRms,
                         micMin,
                         micMax,
                         micHealth);
  }

  if (config::kEnableLaDebugSerial && g_mode == RuntimeMode::kSignal && !g_bootAudioProtocol.active) {
    static uint32_t nextLaDebugMs = 0;
    if (static_cast<int32_t>(nowMs - nextLaDebugMs) >= 0) {
      nextLaDebugMs = nowMs + config::kLaDebugPeriodMs;
      Serial.printf(
          "[LA][MIC] mode=%s det=%u off=%d conf=%u ratio=%.3f mean=%.1f rms=%.1f min=%u max=%u p2p=%u health=%s\n",
          g_micCalibration.active ? "CAL" : "RUN",
          laDetected ? 1U : 0U,
          static_cast<int>(tuningOffset),
          static_cast<unsigned int>(tuningConfidence),
          static_cast<double>(targetRatio),
          static_cast<double>(micMean),
          static_cast<double>(micRms),
          static_cast<unsigned int>(micMin),
          static_cast<unsigned int>(micMax),
          static_cast<unsigned int>(micP2P),
          micHealth);
    }
  }

  if (config::kDisableBoardRgbLeds) {
    g_led.off();
  } else if (g_mode == RuntimeMode::kMp3) {
    if (g_mp3.isPlaying()) {
      g_led.showMp3Playing();
    } else {
      g_led.showMp3Paused();
    }
  } else if (laDetected) {
    g_led.showLaDetected();
  } else {
    g_led.updateRandom(nowMs);
  }

  sendScreenFrameSnapshot(nowMs, screenKey);
}
