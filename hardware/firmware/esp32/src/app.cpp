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
#include "audio/fm_radio_scan_fx.h"
#include "runtime/app_scheduler.h"
#include "runtime/runtime_state.h"

constexpr char kUnlockJingleRtttl[] =
    "zac_unlock:d=16,o=6,b=118:e,p,b,p,e7,8p,e7,b,e7";
constexpr char kWinFallbackJingleRtttl[] =
    "win_8bit:d=16,o=6,b=180:e6,g6,b6,e7,p,e7,d7,b6,g6,e6,2c7";
constexpr uint32_t kBootLoopScanMinMs = 10000U;
constexpr uint32_t kBootLoopScanMaxMs = 40000U;
constexpr uint32_t kEtape2DelayAfterUnlockMs = 15UL * 60UL * 1000UL;

struct BootRadioScanState {
  bool restoreMicCapture = false;
  uint32_t lastLogMs = 0;
};

BootRadioScanState g_bootRadioScan;
FmRadioScanFx g_bootRadioScanFx(config::kPinI2SBclk,
                                config::kPinI2SLrc,
                                config::kPinI2SDout,
                                config::kI2sOutputPort);

struct StoryTimelineState {
  bool unlockArmed = false;
  bool etape2Played = false;
  uint32_t unlockMs = 0;
  uint32_t etape2DueMs = 0;
};

StoryTimelineState g_storyTimeline;

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
bool playRandomTokenFx(const char* token, const char* source, bool allowSdFallback);
void playRtttlJingleBlocking(const char* song, float gain, const char* source);
void resetStoryTimeline(const char* source);
void armStoryTimelineAfterUnlock(uint32_t nowMs);
bool isMp3GateOpen();
void updateStoryTimeline(uint32_t nowMs);
bool startBootRadioScan(const char* source);
void stopBootRadioScan(const char* source);
void updateBootRadioScan(uint32_t nowMs);
void continueAfterBootProtocol(const char* source);
void startBootAudioLoopCycle(uint32_t nowMs, const char* source);
void startMicCalibration(uint32_t nowMs, const char* reason);
bool processCodecDebugCommand(const char* cmd);
void printCodecDebugHelp();
void printMp3DebugHelp();
bool processMp3DebugCommand(const char* cmd, uint32_t nowMs);
void updateMp3FormatTest(uint32_t nowMs);

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

  for (uint32_t i = 0; i < totalSamples; ++i) {
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
    while (!output.ConsumeSample(stereo)) {
      delayMicroseconds(40);
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
  Serial.println("[AUDIO] Boot noise I2S done.");
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
  if (!g_bootRadioScanFx.start()) {
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
  while (decoder->isRunning()) {
    if (!decoder->loop()) {
      break;
    }
    if (maxDurationMs > 0U && static_cast<uint32_t>(millis() - startMs) >= maxDurationMs) {
      timeout = true;
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

  Serial.printf("[AUDIO_FS] %s %s [%s]: %s\n",
                source,
                timeout ? "timeout" : "done",
                bootFsCodecLabel(codec),
                path);
  return !timeout;
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

bool playRandomTokenFx(const char* token, const char* source, bool allowSdFallback) {
  String path;
  if (playRandomLittleFsTokenFx(token,
                                source,
                                config::kBootFxLittleFsGain,
                                config::kBootFxLittleFsMaxDurationMs,
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
                                 config::kBootFxLittleFsMaxDurationMs,
                                 source);
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
  while (g_unlockJinglePlayer.isActive()) {
    g_unlockJinglePlayer.update();
    if (static_cast<int32_t>(millis() - timeoutMs) >= 0) {
      Serial.printf("[AUDIO] %s RTTTL timeout.\n", source);
      break;
    }
    delay(0);
  }
  g_unlockJinglePlayer.stop();

  if (shouldRestoreMicCapture) {
    g_laDetector.setCaptureEnabled(true);
  }
}

void resetStoryTimeline(const char* source) {
  g_storyTimeline.unlockArmed = false;
  g_storyTimeline.etape2Played = false;
  g_storyTimeline.unlockMs = 0;
  g_storyTimeline.etape2DueMs = 0;
  Serial.printf("[STORY] reset (%s)\n", source);
}

void armStoryTimelineAfterUnlock(uint32_t nowMs) {
  g_storyTimeline.unlockArmed = true;
  g_storyTimeline.etape2Played = false;
  g_storyTimeline.unlockMs = nowMs;
  g_storyTimeline.etape2DueMs = nowMs + kEtape2DelayAfterUnlockMs;
  Serial.printf("[STORY] unlock armed: ETAPE_2 due in %lus\n",
                static_cast<unsigned long>(kEtape2DelayAfterUnlockMs / 1000UL));
}

bool isMp3GateOpen() {
  return !g_storyTimeline.unlockArmed || g_storyTimeline.etape2Played;
}

void updateStoryTimeline(uint32_t nowMs) {
  if (!g_storyTimeline.unlockArmed || g_storyTimeline.etape2Played) {
    return;
  }
  if (static_cast<int32_t>(nowMs - g_storyTimeline.etape2DueMs) < 0) {
    return;
  }

  Serial.println("[STORY] ETAPE_2 trigger (T+15min after unlock).");
  const bool played = playRandomTokenFx("ETAPE_2", "story_etape2", true);
  if (!played) {
    Serial.println("[STORY] ETAPE_2 absent: passage sans audio.");
  }
  g_storyTimeline.etape2Played = true;
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

  for (uint32_t i = 0; i < totalSamples; ++i) {
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
    while (!output.ConsumeSample(stereo)) {
      delayMicroseconds(40);
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

  Serial.printf("[AUDIO_DBG] %s tone done freq=%.1fHz gain=%.2f dur=%ums\n",
                source,
                static_cast<double>(freqHz),
                static_cast<double>(gain),
                static_cast<unsigned int>(durationMs));
}

void playBootAudioDiagSequence() {
  Serial.println("[AUDIO_DBG] Diag sequence: 220Hz -> 440Hz -> 880Hz");
  playBootI2sToneFx(220.0f, 260U, 0.28f, "diag_220");
  delay(70);
  playBootI2sToneFx(440.0f, 260U, 0.46f, "diag_440");
  delay(70);
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
  (void)nowMs;
  if (!g_bootAudioProtocol.active) {
    return;
  }

  ++g_bootAudioProtocol.replayCount;
  Serial.printf("[BOOT_PROTO] LOOP #%u via=%s\n",
                static_cast<unsigned int>(g_bootAudioProtocol.replayCount),
                source);

  stopBootRadioScan("boot_proto_cycle");
  if (!playRandomTokenFx("BOOT", source, false)) {
    Serial.println("[BOOT_PROTO] Aucun fichier contenant 'BOOT': fallback FX standard.");
    playBootAudioPrimaryFx(source);
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
  Serial.println("[BOOT_PROTO] Alias: NEXT | OK | SKIP | REPLAY | STATUS | HELP | PAINV | FS_INFO | FS_LIST | FSTEST");
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
  Serial.printf("[MODE] U_LOCK -> detection LA activee (%s)\n", source);
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

  Serial.printf("[BOOT_PROTO] STATUS via=%s waiting_key=1 loops=%u scan=%u left=%lus mode=%s\n",
                source,
                static_cast<unsigned int>(g_bootAudioProtocol.replayCount),
                g_bootRadioScanFx.isActive() ? 1U : 0U,
                static_cast<unsigned long>(leftMs / 1000UL),
                runtimeModeLabel());
}

void finishBootAudioValidationProtocol(const char* reason, bool validated) {
  if (!g_bootAudioProtocol.active) {
    return;
  }

  stopBootRadioScan("boot_proto_finish");
  g_bootAudioProtocol.active = false;
  g_bootAudioProtocol.validated = validated;
  g_bootAudioProtocol.deadlineMs = 0;
  g_bootAudioProtocol.nextReminderMs = 0;
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
  g_bootAudioProtocol.replayCount = 0;
  g_bootAudioProtocol.deadlineMs = 0;
  g_bootAudioProtocol.nextReminderMs = nowMs + static_cast<uint32_t>(config::kBootProtocolPromptPeriodMs);
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
      if (!playRandomTokenFx("BOOT", "serial_boot_replay_manual", false)) {
        playBootAudioPrimaryFx("serial_boot_replay_manual");
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
      if (!playRandomTokenFx("BOOT", "serial_boot_ko_manual", false)) {
        playBootAudioPrimaryFx("serial_boot_ko_manual");
      }
      printBootAudioProtocolStatus(nowMs, "serial_boot_ko_manual");
    }
    return;
  }
  if (strcmp(cmd, "BOOT_TEST_TONE") == 0 || strcmp(cmd, "TONE") == 0) {
    if (protocolActive) {
      stopBootRadioScan("serial_test_tone");
    }
    playBootI2sToneFx(440.0f, 700U, 0.68f, "serial_test_tone");
    if (protocolActive) {
      if (startBootRadioScan("serial_test_tone_resume")) {
        armBootAudioLoopScanWindow(millis(), "serial_test_tone_resume");
      }
    }
    extendBootAudioProtocolWindow(nowMs);
    printBootAudioProtocolStatus(nowMs, "serial_test_tone");
    return;
  }
  if (strcmp(cmd, "BOOT_TEST_DIAG") == 0 || strcmp(cmd, "DIAG") == 0) {
    if (protocolActive) {
      stopBootRadioScan("serial_test_diag");
    }
    playBootAudioDiagSequence();
    if (protocolActive) {
      if (startBootRadioScan("serial_test_diag_resume")) {
        armBootAudioLoopScanWindow(millis(), "serial_test_diag_resume");
      }
    }
    extendBootAudioProtocolWindow(nowMs);
    printBootAudioProtocolStatus(nowMs, "serial_test_diag");
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
    playBootLittleFsFx("serial_boot_fs_test");
    if (protocolActive) {
      if (startBootRadioScan("serial_boot_fs_test_resume")) {
        armBootAudioLoopScanWindow(millis(), "serial_boot_fs_test_resume");
      }
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

  pollBootAudioProtocolSerial(nowMs);
  if (!g_bootAudioProtocol.active) {
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

void printMp3DebugHelp() {
  Serial.println("[MP3_DBG] Cmd: MP3_STATUS | MP3_UNLOCK | MP3_REFRESH | MP3_LIST");
  Serial.println("[MP3_DBG] Cmd: MP3_NEXT | MP3_PREV | MP3_RESTART | MP3_PLAY n");
  Serial.println("[MP3_DBG] Cmd: MP3_TEST_START [ms] | MP3_TEST_STOP");
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
  Serial.printf(
      "[MP3_DBG] %s mode=%s u_son=%u sd=%u tracks=%u cur=%u play=%u pause=%u repeat=%s vol=%u%% file=%s\n",
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

  fs::File root = SD_MMC.open("/");
  if (!root || !root.isDirectory()) {
    Serial.printf("[MP3_DBG] %s list refused: root SD inaccessible.\n", source);
    return;
  }

  uint16_t count = 0;
  Serial.printf("[MP3_DBG] %s list tracks:\n", source);
  fs::File file = root.openNextFile();
  while (file) {
    if (!file.isDirectory()) {
      String path = String(file.name());
      if (!path.startsWith("/")) {
        path = "/" + path;
      }
      const BootFsCodec codec = bootFsCodecFromPath(path.c_str());
      if (codec != BootFsCodec::kUnknown) {
        ++count;
        Serial.printf("[MP3_DBG]   %u [%s] %s size=%u\n",
                      static_cast<unsigned int>(count),
                      bootFsCodecLabel(codec),
                      path.c_str(),
                      static_cast<unsigned int>(file.size()));
      }
    }
    file.close();
    file = root.openNextFile();
  }
  root.close();
  Serial.printf("[MP3_DBG] %s list done count=%u\n",
                source,
                static_cast<unsigned int>(count));
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

    uint16_t guard = 0;
    while (g_mp3.currentTrackNumber() != static_cast<uint16_t>(trackNum) && guard < (count + 1U)) {
      g_mp3.nextTrack();
      ++guard;
    }
    g_mp3.restartTrack();
    printMp3Status("play");
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

    uint16_t guard = 0;
    while (g_mp3.currentTrackNumber() > 1U && guard < (g_mp3.trackCount() + 1U)) {
      g_mp3.previousTrack();
      ++guard;
    }
    g_mp3.restartTrack();

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
  g_mp3.restartTrack();
  g_mp3FormatTest.stageStartMs = nowMs;
  g_mp3FormatTest.stageResultLogged = false;
}

void printKeyTuneHelp() {
  Serial.println("[KEY_TUNE] Cmd: KEY_STATUS | KEY_RAW_ON | KEY_RAW_OFF | KEY_RESET");
  Serial.println("[KEY_TUNE] Cmd: KEY_SET K4 1500 | KEY_SET K6 2200 | KEY_SET REL 3920");
  Serial.println("[KEY_TUNE] Cmd: KEY_SET_ALL k1 k2 k3 k4 k5 k6 rel");
  Serial.println("[KEY_TUNE] Cmd: KEY_TEST_START | KEY_TEST_STATUS | KEY_TEST_RESET | KEY_TEST_STOP");
  Serial.println("[KEY_TUNE] Cmd: FS_INFO | FS_LIST | FSTEST");
  Serial.println("[KEY_TUNE] Cmd: CODEC_STATUS | CODEC_DUMP | CODEC_RD/WR | CODEC_VOL");
  Serial.println("[KEY_TUNE] Cmd: MP3_STATUS | MP3_UNLOCK | MP3_REFRESH | MP3_LIST | MP3_TEST_START");
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
                         strcmp(cmd, "FS_LIST") == 0 || strcmp(cmd, "FSTEST") == 0;
  if (strncmp(cmd, "BOOT_", 5U) == 0 || bootAlias) {
    processBootAudioSerialCommand(cmd, nowMs);
    return;
  }

  if (processCodecDebugCommand(cmd)) {
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
    g_laDetectionEnabled = false;
    g_laDetector.setCaptureEnabled(false);
    g_sine.setEnabled(false);
    if (changed) {
      Serial.println("[MODE] LECTEUR U-SON (SD detectee)");
    }
  } else {
    stopUnlockJingle(false);
    g_uSonFunctional = false;
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
    switch (key) {
      case 1:
        g_mp3.togglePause();
        Serial.printf("[KEY] K1 MP3 %s\n", g_mp3.isPaused() ? "PAUSE" : "PLAY");
        break;
      case 2:
        g_mp3.previousTrack();
        Serial.printf("[KEY] K2 PREV %u/%u\n",
                      static_cast<unsigned int>(g_mp3.currentTrackNumber()),
                      static_cast<unsigned int>(g_mp3.trackCount()));
        break;
      case 3:
        g_mp3.nextTrack();
        Serial.printf("[KEY] K3 NEXT %u/%u\n",
                      static_cast<unsigned int>(g_mp3.currentTrackNumber()),
                      static_cast<unsigned int>(g_mp3.trackCount()));
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
        g_mp3.cycleRepeatMode();
        Serial.printf("[KEY] K6 REPEAT %s\n", g_mp3.repeatModeLabel());
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
      Serial.println("[KEY] K2 I2S tone 440Hz.");
      playBootI2sToneFx(440.0f, 650U, 0.68f, "key_k2_i2s_tone");
      break;
    case 3:
      Serial.println("[KEY] K3 I2S diag sequence.");
      playBootAudioDiagSequence();
      break;
    case 4:
      Serial.println("[KEY] K4 I2S boot FX replay.");
      playBootAudioPrimaryFx("key_k4_replay");
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
  g_keypad.begin();
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
  g_screen.begin();
  g_paEnableActiveHigh = config::kPinAudioPaEnableActiveHigh;
  if (config::kBootAudioPaTogglePulse && config::kPinAudioPaEnable >= 0) {
    setBootAudioPaEnabled(false, "boot_pa_pulse_off");
    delay(config::kBootAudioPaToggleMs);
  }
  setBootAudioPaEnabled(true, "boot_setup");
  printBootAudioOutputInfo("boot_setup");
  g_sine.setEnabled(false);
  applyRuntimeMode(schedulerSelectRuntimeMode(makeSchedulerInputs()), true);
  startBootAudioValidationProtocol(millis());

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
  Serial.println("[STORY] Fin U_LOCK: lecture random '*WIN*' (fallback jingle 8-bit WIN).");
  Serial.println("[STORY] Fin U-SON: lecture random '*ETAPE_2*' a T+15min apres unlock.");
  Serial.println("[BOOT] En U_LOCK: detection SD desactivee jusqu'au mode U-SON Fonctionnel.");
  if (config::kEnableBootAudioValidationProtocol) {
    Serial.println("[KEYMAP][BOOT_PROTO] K1..K6=NEXT | Serial: BOOT_NEXT, BOOT_REPLAY, BOOT_REOPEN");
  }
  Serial.println(
      "[KEY_TUNE] Serial: KEY_STATUS | KEY_RAW_ON/OFF | KEY_SET Kx/REL v | KEY_TEST_START/STATUS/RESET/STOP");
  Serial.println(
      "[MP3_DBG] Serial: MP3_STATUS | MP3_UNLOCK | MP3_REFRESH | MP3_LIST | MP3_PLAY n | MP3_TEST_START [ms]");
  Serial.println("[FS] Serial: FS_INFO | FS_LIST | FSTEST");
  Serial.printf("[FS] Boot FX path: %s (%s)\n",
                config::kBootFxLittleFsPath,
                config::kPreferLittleFsBootFx ? "preferred" : "disabled");
  Serial.println("[KEYMAP][SIGNAL] actifs seulement apres unlock: K1 LA on/off, K2 tone 440 I2S, K3 diag I2S, K4 replay FX I2S, K5 refresh SD, K6 cal micro");
}

void App::loop() {
  const uint32_t nowMs = millis();
  updateStoryTimeline(nowMs);
  AppSchedulerInputs schedulerInput = makeSchedulerInputs();
  AppBrickSchedule schedule = schedulerBuildBricks(schedulerInput);

  if (schedule.runBootProtocol) {
    updateBootAudioValidationProtocol(nowMs);
  }
  if (schedule.runSerialConsole) {
    pollKeyTuneSerial(nowMs);
  }

  schedulerInput = makeSchedulerInputs();
  schedule = schedulerBuildBricks(schedulerInput);

  if (schedule.runUnlockJingle) {
    updateUnlockJingle(nowMs);
  }

  if (schedule.runMp3Service) {
    g_mp3.update(nowMs, schedule.allowMp3Playback);
  }
  applyRuntimeMode(schedulerSelectRuntimeMode(makeSchedulerInputs()));
  updateMp3FormatTest(nowMs);

  const AppBrickSchedule postModeSchedule = schedulerBuildBricks(makeSchedulerInputs());
  if (postModeSchedule.runSineDac) {
    g_sine.update();
  }
  if (postModeSchedule.runLaDetector) {
    g_laDetector.update(nowMs);
  }
  g_keypad.update(nowMs);

  static uint8_t screenKey = 0;
  static uint32_t screenKeyUntilMs = 0;
  uint8_t pressedKey = 0;
  uint16_t pressedRaw = 0;
  if (g_keypad.consumePress(&pressedKey, &pressedRaw)) {
    Serial.printf("[KEY] K%u raw=%u\n", static_cast<unsigned int>(pressedKey), pressedRaw);
    if (g_bootAudioProtocol.active) {
      handleBootAudioProtocolKey(pressedKey, nowMs);
    } else if (g_keySelfTest.active) {
      handleKeySelfTestPress(pressedKey, pressedRaw);
    } else {
      handleKeyPress(pressedKey);
    }
    screenKey = pressedKey;
    screenKeyUntilMs = nowMs + 1200;
  }
  if (screenKey != 0 && static_cast<int32_t>(nowMs - screenKeyUntilMs) >= 0) {
    screenKey = 0;
  }
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
    resetLaHoldProgress();
    armStoryTimelineAfterUnlock(nowMs);
    g_mp3.requestStorageRefresh();
    if (!playRandomTokenFx("WIN", "unlock_win", true)) {
      Serial.println("[STORY] Aucun fichier contenant 'WIN': fallback jingle 8-bit I2S.");
      playRtttlJingleBlocking(kWinFallbackJingleRtttl,
                              config::kUnlockI2sJingleGain,
                              "unlock_win_fallback");
    }
    Serial.println("[MODE] MODULE U-SON Fonctionnel (LA detecte)");
    Serial.println("[SD] Detection SD activee.");
  }

  const bool uLockMode = (g_mode == RuntimeMode::kSignal) && !g_uSonFunctional;
  const bool uLockListening = uLockMode && g_uLockListening;
  const bool uSonFunctional = (g_mode == RuntimeMode::kSignal) && g_uSonFunctional;
  const uint8_t laHoldPercent = uLockListening ? laHoldPercentBeforeUnlock : 0;
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

  g_screen.update(laDetected,
                  g_mp3.isPlaying(),
                  g_mp3.isSdReady(),
                  g_mode == RuntimeMode::kMp3,
                  uLockMode,
                  uLockListening,
                  uSonFunctional,
                  screenKey,
                  g_mp3.currentTrackNumber(),
                  g_mp3.trackCount(),
                  g_mp3.volumePercent(),
                  micLevelPercent,
                  tuningOffset,
                  tuningConfidence,
                  config::kScreenEnableMicScope && config::kUseI2SMicInput,
                  laHoldPercent,
                  static_cast<uint8_t>(currentStartupStage()),
                  static_cast<uint8_t>(currentAppStage()),
                  nowMs);
}
