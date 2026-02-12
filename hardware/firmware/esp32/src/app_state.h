#pragma once

#include <Arduino.h>

#include "config.h"
#include "i2s_jingle_player.h"
#include "keypad_analog.h"
#include "la_detector.h"
#include "led_controller.h"
#include "mp3_player.h"
#include "screen_link.h"
#include "sine_dac.h"

enum class RuntimeMode : uint8_t {
  kSignal = 0,
  kMp3 = 1,
};

extern LedController g_led;
extern LaDetector g_laDetector;
extern SineDac g_sine;
extern KeypadAnalog g_keypad;
extern ScreenLink g_screen;
extern Mp3Player g_mp3;
extern I2sJinglePlayer g_unlockJinglePlayer;

extern RuntimeMode g_mode;
extern bool g_laDetectionEnabled;
extern bool g_uSonFunctional;
extern bool g_uLockListening;
extern uint32_t g_laHoldAccumMs;
extern uint32_t g_lastLoopMs;
extern bool g_paEnableActiveHigh;
extern bool g_paEnabledRequest;
extern bool g_littleFsReady;

struct UnlockJingleState {
  bool active = false;
  bool restoreMicCapture = false;
};
extern UnlockJingleState g_unlockJingle;

struct BootAudioProtocolState {
  bool active = false;
  bool validated = false;
  uint8_t replayCount = 0;
  uint32_t deadlineMs = 0;
  uint32_t nextReminderMs = 0;
  char serialCmdBuffer[32] = {};
  uint8_t serialCmdLen = 0;
};
extern BootAudioProtocolState g_bootAudioProtocol;

struct KeyTuneState {
  bool rawStreamEnabled = false;
  uint32_t nextRawLogMs = 0;
  char serialCmdBuffer[80] = {};
  uint8_t serialCmdLen = 0;
};
extern KeyTuneState g_keyTune;

struct KeySelfTestState {
  bool active = false;
  bool seen[6] = {false, false, false, false, false, false};
  uint16_t rawMin[6] = {0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF};
  uint16_t rawMax[6] = {0, 0, 0, 0, 0, 0};
  uint8_t seenCount = 0;
};
extern KeySelfTestState g_keySelfTest;

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
extern MicCalibrationState g_micCalibration;

struct Mp3FormatTestState {
  bool active = false;
  uint16_t totalTracks = 0;
  uint16_t testedTracks = 0;
  uint16_t okTracks = 0;
  uint16_t failTracks = 0;
  uint32_t dwellMs = 3500;
  uint32_t stageStartMs = 0;
  bool stageResultLogged = false;
};
extern Mp3FormatTestState g_mp3FormatTest;
