#include <Arduino.h>
#include <cstring>
#include <cmath>
#include <cctype>
#include <cstdio>

#include <AudioOutputI2S.h>

#include "config.h"
#include "i2s_jingle_player.h"
#include "keypad_analog.h"
#include "la_detector.h"
#include "led_controller.h"
#include "mp3_player.h"
#include "screen_link.h"
#include "sine_dac.h"

LedController g_led(config::kPinLedR, config::kPinLedG, config::kPinLedB);
LaDetector g_laDetector(config::kPinMicAdc,
                        config::kUseI2SMicInput,
                        config::kPinMicI2SBclk,
                        config::kPinMicI2SLrc,
                        config::kPinMicI2SDin);
SineDac g_sine(config::kPinDacSine, config::kSineFreqHz, config::kDacSampleRate);
KeypadAnalog g_keypad(config::kPinKeysAdc);
ScreenLink g_screen(Serial2,
                    config::kPinScreenTx,
                    config::kScreenBaud,
                    config::kScreenUpdatePeriodMs);
Mp3Player g_mp3(config::kPinI2SBclk,
                config::kPinI2SLrc,
                config::kPinI2SDout,
                config::kMp3Path,
                config::kPinAudioPaEnable);
I2sJinglePlayer g_unlockJinglePlayer(config::kPinI2SBclk,
                                     config::kPinI2SLrc,
                                     config::kPinI2SDout,
                                     config::kI2sOutputPort);

enum class RuntimeMode : uint8_t {
  kSignal = 0,
  kMp3 = 1,
};

RuntimeMode g_mode = RuntimeMode::kSignal;
bool g_laDetectionEnabled = true;
bool g_uSonFunctional = false;
bool g_uLockListening = false;
uint32_t g_laHoldAccumMs = 0;
uint32_t g_lastLoopMs = 0;

constexpr char kUnlockJingleRtttl[] =
    "zac_unlock:d=16,o=6,b=118:e,p,b,p,e7,8p,e7,b,e7";

struct UnlockJingleState {
  bool active = false;
  bool restoreMicCapture = false;
};

UnlockJingleState g_unlockJingle;

struct BootAudioProtocolState {
  bool active = false;
  bool validated = false;
  uint8_t replayCount = 0;
  uint32_t deadlineMs = 0;
  uint32_t nextReminderMs = 0;
  char serialCmdBuffer[32] = {};
  uint8_t serialCmdLen = 0;
};

BootAudioProtocolState g_bootAudioProtocol;

struct KeyTuneState {
  bool rawStreamEnabled = false;
  uint32_t nextRawLogMs = 0;
  char serialCmdBuffer[80] = {};
  uint8_t serialCmdLen = 0;
};

KeyTuneState g_keyTune;

struct KeySelfTestState {
  bool active = false;
  bool seen[6] = {false, false, false, false, false, false};
  uint16_t rawMin[6] = {0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF};
  uint16_t rawMax[6] = {0, 0, 0, 0, 0, 0};
  uint8_t seenCount = 0;
};

KeySelfTestState g_keySelfTest;

struct MicCalibrationState {
  bool active = false;
  uint32_t untilMs = 0;
  uint32_t nextLogMs = 0;
  uint32_t samples = 0;
  float rmsMin = 1000000.0f;
  float rmsMax = 0.0f;
  float ratioMin = 1000000.0f;
  float ratioMax = 0.0f;
  uint16_t p2pMin = 0xFFFF;
  uint16_t p2pMax = 0;
  uint16_t okCount = 0;
  uint16_t silenceCount = 0;
  uint16_t saturationCount = 0;
  uint16_t tooLoudCount = 0;
  uint16_t detectOffCount = 0;
};

MicCalibrationState g_micCalibration;

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

void setBootAudioPaEnabled(bool enabled, const char* source) {
  if (config::kPinAudioPaEnable < 0) {
    return;
  }
  pinMode(static_cast<uint8_t>(config::kPinAudioPaEnable), OUTPUT);
  digitalWrite(static_cast<uint8_t>(config::kPinAudioPaEnable), enabled ? HIGH : LOW);
  Serial.printf("[AUDIO_DBG] %s PA_EN=%s pin=%d\n",
                source,
                enabled ? "HIGH" : "LOW",
                static_cast<int>(config::kPinAudioPaEnable));
}

void printBootAudioOutputInfo(const char* source) {
  int paState = -1;
  if (config::kPinAudioPaEnable >= 0) {
    paState = digitalRead(static_cast<uint8_t>(config::kPinAudioPaEnable));
  }

  Serial.printf(
      "[AUDIO_DBG] %s i2s_port=%u bclk=%u lrc=%u dout=%u sr=%u boot_gain=%.2f pa_state=%d\n",
      source,
      static_cast<unsigned int>(config::kI2sOutputPort),
      static_cast<unsigned int>(config::kPinI2SBclk),
      static_cast<unsigned int>(config::kPinI2SLrc),
      static_cast<unsigned int>(config::kPinI2SDout),
      static_cast<unsigned int>(config::kBootI2sNoiseSampleRateHz),
      static_cast<double>(config::kBootI2sNoiseGain),
      paState);
}

void extendBootAudioProtocolWindow(uint32_t nowMs) {
  if (!g_bootAudioProtocol.active) {
    return;
  }
  g_bootAudioProtocol.deadlineMs = nowMs + config::kBootAudioValidationTimeoutMs;
  g_bootAudioProtocol.nextReminderMs = nowMs + 2500U;
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
  playBootI2sToneFx(220.0f, 260U, 0.14f, "diag_220");
  delay(70);
  playBootI2sToneFx(440.0f, 260U, 0.20f, "diag_440");
  delay(70);
  playBootI2sToneFx(880.0f, 260U, 0.26f, "diag_880");
}

void printBootAudioProtocolHelp() {
  Serial.println("[BOOT_PROTO] Touches: K1=OK, K2=REPLAY, K3=KO+REPLAY, K4=TONE, K5=DIAG, K6=SKIP");
  Serial.println(
      "[BOOT_PROTO] Serial: BOOT_OK | BOOT_REPLAY | BOOT_KO | BOOT_SKIP | BOOT_STATUS | BOOT_HELP");
  Serial.println("[BOOT_PROTO] Serial: BOOT_TEST_TONE | BOOT_TEST_DIAG | BOOT_PA_ON | BOOT_PA_OFF | BOOT_PA_STATUS");
  Serial.println("[BOOT_PROTO] Alias: OK | REPLAY | KO | SKIP | STATUS | HELP");
}

uint32_t bootAudioProtocolTimeLeftMs(uint32_t nowMs) {
  if (!g_bootAudioProtocol.active) {
    return 0;
  }
  if (static_cast<int32_t>(g_bootAudioProtocol.deadlineMs - nowMs) <= 0) {
    return 0;
  }
  return g_bootAudioProtocol.deadlineMs - nowMs;
}

void printBootAudioProtocolStatus(uint32_t nowMs, const char* source) {
  if (!g_bootAudioProtocol.active) {
    Serial.printf("[BOOT_PROTO] STATUS via=%s inactive validated=%u\n",
                  source,
                  g_bootAudioProtocol.validated ? 1U : 0U);
    return;
  }

  Serial.printf("[BOOT_PROTO] STATUS via=%s left=%lu ms replay=%u/%u\n",
                source,
                static_cast<unsigned long>(bootAudioProtocolTimeLeftMs(nowMs)),
                static_cast<unsigned int>(g_bootAudioProtocol.replayCount),
                static_cast<unsigned int>(config::kBootAudioValidationMaxReplays));
}

void finishBootAudioValidationProtocol(const char* reason, bool validated) {
  if (!g_bootAudioProtocol.active) {
    return;
  }

  g_bootAudioProtocol.active = false;
  g_bootAudioProtocol.validated = validated;
  g_bootAudioProtocol.nextReminderMs = 0;
  Serial.printf("[BOOT_PROTO] DONE status=%s reason=%s replay=%u\n",
                validated ? "VALIDATED" : "BYPASS",
                reason,
                static_cast<unsigned int>(g_bootAudioProtocol.replayCount));
}

void replayBootAudioProtocolFx(uint32_t nowMs, const char* source) {
  if (!g_bootAudioProtocol.active) {
    return;
  }

  if (g_bootAudioProtocol.replayCount >= config::kBootAudioValidationMaxReplays) {
    Serial.println("[BOOT_PROTO] REPLAY refuse: max atteint.");
    return;
  }

  ++g_bootAudioProtocol.replayCount;
  Serial.printf("[BOOT_PROTO] REPLAY #%u via %s\n",
                static_cast<unsigned int>(g_bootAudioProtocol.replayCount),
                source);
  playBootI2sNoiseFx();
  g_bootAudioProtocol.deadlineMs = nowMs + config::kBootAudioValidationTimeoutMs;
  g_bootAudioProtocol.nextReminderMs = nowMs + 2500U;
  printBootAudioProtocolStatus(nowMs, source);
}

void startBootAudioValidationProtocol(uint32_t nowMs) {
  if (!config::kEnableBootAudioValidationProtocol || !config::kEnableBootI2sNoiseFx) {
    return;
  }

  g_bootAudioProtocol.active = true;
  g_bootAudioProtocol.validated = false;
  g_bootAudioProtocol.replayCount = 0;
  g_bootAudioProtocol.deadlineMs = nowMs + config::kBootAudioValidationTimeoutMs;
  g_bootAudioProtocol.nextReminderMs = nowMs + 2500U;
  g_bootAudioProtocol.serialCmdLen = 0;
  g_bootAudioProtocol.serialCmdBuffer[0] = '\0';

  Serial.printf("[BOOT_PROTO] START timeout=%lu ms max_replays=%u\n",
                static_cast<unsigned long>(config::kBootAudioValidationTimeoutMs),
                static_cast<unsigned int>(config::kBootAudioValidationMaxReplays));
  printBootAudioProtocolStatus(nowMs, "start");
  printBootAudioProtocolHelp();
}

void processBootAudioSerialCommand(const char* rawCmd, uint32_t nowMs) {
  if (!g_bootAudioProtocol.active || rawCmd == nullptr || rawCmd[0] == '\0') {
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

  if (strcmp(cmd, "BOOT_OK") == 0 || strcmp(cmd, "OK") == 0 || strcmp(cmd, "VALID") == 0) {
    finishBootAudioValidationProtocol("serial_boot_ok", true);
    return;
  }
  if (strcmp(cmd, "BOOT_REPLAY") == 0 || strcmp(cmd, "REPLAY") == 0 || strcmp(cmd, "R") == 0) {
    replayBootAudioProtocolFx(nowMs, "serial_boot_replay");
    return;
  }
  if (strcmp(cmd, "BOOT_KO") == 0 || strcmp(cmd, "KO") == 0 || strcmp(cmd, "NOK") == 0) {
    Serial.println("[BOOT_PROTO] KO recu (serial), relecture.");
    replayBootAudioProtocolFx(nowMs, "serial_boot_ko");
    return;
  }
  if (strcmp(cmd, "BOOT_TEST_TONE") == 0 || strcmp(cmd, "TONE") == 0) {
    playBootI2sToneFx(440.0f, 700U, 0.22f, "serial_test_tone");
    extendBootAudioProtocolWindow(millis());
    printBootAudioProtocolStatus(millis(), "serial_test_tone");
    return;
  }
  if (strcmp(cmd, "BOOT_TEST_DIAG") == 0 || strcmp(cmd, "DIAG") == 0) {
    playBootAudioDiagSequence();
    extendBootAudioProtocolWindow(millis());
    printBootAudioProtocolStatus(millis(), "serial_test_diag");
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
  if (strcmp(cmd, "BOOT_SKIP") == 0 || strcmp(cmd, "SKIP") == 0) {
    finishBootAudioValidationProtocol("serial_boot_skip", false);
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
  if (!g_bootAudioProtocol.active) {
    return;
  }

  switch (key) {
    case 1:
      finishBootAudioValidationProtocol("key_k1_ok", true);
      break;
    case 2:
      replayBootAudioProtocolFx(nowMs, "key_k2_replay");
      break;
    case 3:
      Serial.println("[BOOT_PROTO] KO recu (K3), relecture.");
      replayBootAudioProtocolFx(nowMs, "key_k3_ko");
      break;
    case 4:
      Serial.println("[BOOT_PROTO] K4 test tone 440Hz.");
      playBootI2sToneFx(440.0f, 700U, 0.22f, "key_k4_tone");
      extendBootAudioProtocolWindow(millis());
      printBootAudioProtocolStatus(millis(), "key_k4_tone");
      break;
    case 5:
      Serial.println("[BOOT_PROTO] K5 diag sequence.");
      playBootAudioDiagSequence();
      extendBootAudioProtocolWindow(millis());
      printBootAudioProtocolStatus(millis(), "key_k5_diag");
      break;
    case 6:
      finishBootAudioValidationProtocol("key_k6_skip", false);
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
  if (static_cast<int32_t>(nowMs - g_bootAudioProtocol.deadlineMs) >= 0) {
    Serial.println("[BOOT_PROTO] TIMEOUT -> SKIP auto.");
    finishBootAudioValidationProtocol("timeout_auto_skip", false);
    return;
  }

  if (static_cast<int32_t>(nowMs - g_bootAudioProtocol.nextReminderMs) >= 0) {
    printBootAudioProtocolStatus(nowMs, "tick");
    Serial.println("[BOOT_PROTO] Attente validation: K1 OK, K2 replay, K4 tone, K5 diag.");
    g_bootAudioProtocol.nextReminderMs = nowMs + 2500U;
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

void printKeyTuneHelp() {
  Serial.println("[KEY_TUNE] Cmd: KEY_STATUS | KEY_RAW_ON | KEY_RAW_OFF | KEY_RESET");
  Serial.println("[KEY_TUNE] Cmd: KEY_SET K4 1500 | KEY_SET K6 2200 | KEY_SET REL 3920");
  Serial.println("[KEY_TUNE] Cmd: KEY_SET_ALL k1 k2 k3 k4 k5 k6 rel");
  Serial.println("[KEY_TUNE] Cmd: KEY_TEST_START | KEY_TEST_STATUS | KEY_TEST_RESET | KEY_TEST_STOP");
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

RuntimeMode selectRuntimeMode() {
  // Tant qu'on est en U_LOCK ou en jingle d'unlock, on n'active pas la detection SD/MP3.
  if (g_mode == RuntimeMode::kSignal && (!g_uSonFunctional || g_unlockJingle.active)) {
    return RuntimeMode::kSignal;
  }

  if (g_mp3.isSdReady() && g_mp3.hasTracks()) {
    return RuntimeMode::kMp3;
  }
  return RuntimeMode::kSignal;
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
    case 2: {
      const float newFreq =
          max(config::kSineFreqMinHz, g_sine.frequency() - config::kSineFreqStepHz);
      g_sine.setFrequency(newFreq);
      Serial.printf("[KEY] K2 SINE %.1f Hz\n", static_cast<double>(g_sine.frequency()));
      break;
    }
    case 3: {
      const float newFreq =
          min(config::kSineFreqMaxHz, g_sine.frequency() + config::kSineFreqStepHz);
      g_sine.setFrequency(newFreq);
      Serial.printf("[KEY] K3 SINE %.1f Hz\n", static_cast<double>(g_sine.frequency()));
      break;
    }
    case 4:
      if (config::kEnableSineDac) {
        g_sine.setEnabled(!g_sine.isEnabled());
        Serial.printf("[KEY] K4 SINE %s\n", g_sine.isEnabled() ? "ON" : "OFF");
      } else {
        Serial.println("[KEY] K4 SINE indisponible.");
      }
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

void setup() {
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
  g_mp3.begin();
  g_screen.begin();
  setBootAudioPaEnabled(true, "boot_setup");
  printBootAudioOutputInfo("boot_setup");
  g_sine.setEnabled(false);
  applyRuntimeMode(selectRuntimeMode(), true);
  playBootI2sNoiseFx();
  startBootAudioValidationProtocol(millis());

  Serial.println("[BOOT] U-SON / ESP32 Audio Kit A252 pret.");
  if (config::kDisableBoardRgbLeds) {
    Serial.println("[LED] RGB carte force OFF.");
  }
  Serial.printf("[MIC] Source: %s\n",
                config::kUseI2SMicInput ? "I2S codec onboard (DIN GPIO35)" : "ADC externe GPIO34");
  Serial.println("[KEYMAP][MP3] K1 play/pause, K2 prev, K3 next, K4 vol-, K5 vol+, K6 repeat");
  Serial.println("[BOOT] Sans SD: MODE U_LOCK, appuyer une touche pour lancer la detection LA.");
  Serial.println("[BOOT] Puis MODULE U-SON Fonctionnel apres detection LA.");
  Serial.println("[BOOT] En U_LOCK: detection SD desactivee jusqu'au mode U-SON Fonctionnel.");
  if (config::kEnableBootAudioValidationProtocol && config::kEnableBootI2sNoiseFx) {
    Serial.println("[KEYMAP][BOOT_PROTO] K1=OK, K2=REPLAY, K3=KO+REPLAY, K4=TONE, K5=DIAG, K6=SKIP");
  }
  Serial.println(
      "[KEY_TUNE] Serial: KEY_STATUS | KEY_RAW_ON/OFF | KEY_SET Kx/REL v | KEY_TEST_START/STATUS/RESET/STOP");
  Serial.println("[KEYMAP][SIGNAL] actifs seulement apres unlock: K1 LA on/off, K2 sine-, K3 sine+, K4 sine on/off, K5 refresh SD, K6 cal micro");
}

void loop() {
  const uint32_t nowMs = millis();
  updateBootAudioValidationProtocol(nowMs);
  pollKeyTuneSerial(nowMs);

  if (g_mode == RuntimeMode::kSignal) {
    updateUnlockJingle(nowMs);
  }

  // En U_LOCK: ne pas scanner/monter la SD.
  if (g_mode == RuntimeMode::kMp3 || (g_uSonFunctional && !g_unlockJingle.active)) {
    g_mp3.update(nowMs);
  }
  applyRuntimeMode(selectRuntimeMode());

  if (g_mode == RuntimeMode::kSignal && config::kEnableSineDac) {
    g_sine.update();
  }
  if (g_mode == RuntimeMode::kSignal && g_laDetectionEnabled) {
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
    g_mp3.requestStorageRefresh();
    startUnlockJingle(nowMs);
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

  if (config::kEnableLaDebugSerial && g_mode == RuntimeMode::kSignal) {
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
                  nowMs);
}
