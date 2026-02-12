#include <Arduino.h>
#include <cstring>

#include "config.h"
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
  // Tant qu'on est en U_LOCK, on n'active pas la detection SD/MP3.
  if (g_mode == RuntimeMode::kSignal && !g_uSonFunctional) {
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
    stopMicCalibration(millis(), "mode_mp3");
    g_laDetectionEnabled = false;
    g_laDetector.setCaptureEnabled(false);
    g_sine.setEnabled(false);
    if (changed) {
      Serial.println("[MODE] LECTEUR U-SON (SD detectee)");
    }
  } else {
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
  g_sine.setEnabled(false);
  applyRuntimeMode(selectRuntimeMode(), true);

  Serial.println("[BOOT] U-SON / ESP32 Audio Kit A252 pret.");
  Serial.printf("[MIC] Source: %s\n",
                config::kUseI2SMicInput ? "I2S codec onboard (DIN GPIO35)" : "ADC externe GPIO34");
  Serial.println("[KEYMAP][MP3] K1 play/pause, K2 prev, K3 next, K4 vol-, K5 vol+, K6 repeat");
  Serial.println("[BOOT] Sans SD: MODE U_LOCK, appuyer une touche pour lancer la detection LA.");
  Serial.println("[BOOT] Puis MODULE U-SON Fonctionnel apres detection LA.");
  Serial.println("[BOOT] En U_LOCK: detection SD desactivee jusqu'au mode U-SON Fonctionnel.");
  Serial.println("[KEYMAP][SIGNAL] actifs seulement apres unlock: K1 LA on/off, K2 sine-, K3 sine+, K4 sine on/off, K5 refresh SD, K6 cal micro");
}

void loop() {
  const uint32_t nowMs = millis();

  // En U_LOCK: ne pas scanner/monter la SD.
  if (g_mode == RuntimeMode::kMp3 || g_uSonFunctional) {
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
    handleKeyPress(pressedKey);
    screenKey = pressedKey;
    screenKeyUntilMs = nowMs + 1200;
  }
  if (screenKey != 0 && static_cast<int32_t>(nowMs - screenKeyUntilMs) >= 0) {
    screenKey = 0;
  }

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

  if (g_mode == RuntimeMode::kMp3) {
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
