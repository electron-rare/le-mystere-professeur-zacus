#pragma once

#include <Arduino.h>

namespace config {

constexpr uint8_t kPinLedR = 16;
constexpr uint8_t kPinLedG = 17;
constexpr uint8_t kPinLedB = 4;

// Entree analogique micro externe (optionnel sur Audio Kit A252)
constexpr uint8_t kPinMic = 34;

// DAC interne utilise uniquement en mode fallback (sans SD/MP3)
// Note: ce GPIO est partage avec l'I2S codec, donc le sinus est coupe en mode MP3.
constexpr bool kEnableSineDac = true;
constexpr uint8_t kPinDacSine = 26;

// Profil ESP32 Audio Kit V2.2 A252 (codec + SD_MMC)
constexpr uint8_t kPinI2SBclk = 27;
constexpr uint8_t kPinI2SLrc = 25;
constexpr uint8_t kPinI2SDout = 26;
constexpr int8_t kPinAudioPaEnable = 21;

// ESP32 -> ESP8266 (ecran) en UART unidirectionnel
constexpr uint8_t kPinScreenTx = 22;
constexpr uint32_t kScreenBaud = 57600;
constexpr uint16_t kScreenUpdatePeriodMs = 250;

// Clavier analogique (6 touches sur 1 entree ADC)
constexpr uint8_t kPinKeysAdc = 36;
constexpr uint16_t kKeysSampleEveryMs = 8;
constexpr uint16_t kKeysDebounceMs = 30;
constexpr uint16_t kKeysReleaseThreshold = 3800;
constexpr uint16_t kKey1Max = 260;
constexpr uint16_t kKey2Max = 700;
constexpr uint16_t kKey3Max = 1200;
constexpr uint16_t kKey4Max = 1800;
constexpr uint16_t kKey5Max = 2500;
constexpr uint16_t kKey6Max = 3300;

constexpr char kMp3Path[] = "/track001.mp3";

constexpr float kSineFreqHz = 440.0f;
constexpr float kSineFreqStepHz = 20.0f;
constexpr float kSineFreqMinHz = 220.0f;
constexpr float kSineFreqMaxHz = 880.0f;
constexpr uint16_t kDacSampleRate = 22050;

constexpr float kDetectFs = 4000.0f;
constexpr uint16_t kDetectN = 128;
constexpr float kDetectTargetHz = 440.0f;
constexpr float kDetectRatioThreshold = 1.8f;
constexpr uint16_t kDetectEveryMs = 100;
constexpr uint32_t kDetectSamplePeriodUs = static_cast<uint32_t>(1000000.0f / kDetectFs);
constexpr uint8_t kDetectMaxSamplesPerLoop = 8;
constexpr bool kEnableLaDebugSerial = true;
constexpr uint16_t kLaDebugPeriodMs = 250;
constexpr bool kEnableMicCalibrationOnSignalEntry = true;
constexpr uint32_t kMicCalibrationDurationMs = 30000;
constexpr uint16_t kMicCalibrationLogPeriodMs = 500;

}  // namespace config
