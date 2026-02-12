#pragma once

#include <Arduino.h>

namespace config {

constexpr uint8_t kPinLedR = 16;
constexpr uint8_t kPinLedG = 17;
constexpr uint8_t kPinLedB = 4;

// Micro:
// - Profil A252 recommande: micro du codec ES8388 en I2S (DIN GPIO35)
// - Fallback possible: micro analogique externe sur ADC GPIO34
constexpr bool kUseI2SMicInput = true;
constexpr uint8_t kPinMicAdc = 34;
constexpr uint8_t kPinMicI2SBclk = 27;
constexpr uint8_t kPinMicI2SLrc = 25;
constexpr uint8_t kPinMicI2SDin = 35;
constexpr bool kMicI2SUseLeftChannel = true;
constexpr uint8_t kPinCodecI2CScl = 32;
constexpr uint8_t kPinCodecI2CSda = 33;
constexpr uint8_t kCodecI2CAddress = 0x10;  // ES8388 7-bit address
constexpr uint32_t kCodecI2CClockHz = 100000;
constexpr uint8_t kCodecMicGainDb = 24;  // 0..24 dB by 3 dB steps
constexpr bool kCodecMicUseLine2Input = false;
constexpr bool kCodecMicAutoSwitchLineOnSilence = true;
constexpr uint16_t kCodecMicSilenceP2PThreshold = 6;
constexpr uint32_t kCodecMicSilenceSwitchMs = 3000;
// Alias pour compatibilite avec le code existant.
constexpr uint8_t kPinMic = kPinMicAdc;

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
constexpr uint32_t kScreenBaud = 38400;
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
constexpr float kDetectMinRmsForDetection = 6.0f;
constexpr uint16_t kDetectMinP2PForDetection = 24;
constexpr uint16_t kDetectEveryMs = 100;
constexpr uint32_t kDetectSamplePeriodUs = static_cast<uint32_t>(1000000.0f / kDetectFs);
constexpr uint8_t kDetectMaxSamplesPerLoop = 8;
constexpr bool kEnableLaDebugSerial = true;
constexpr uint16_t kLaDebugPeriodMs = 250;
constexpr bool kEnableMicCalibrationOnSignalEntry = true;
constexpr uint32_t kMicCalibrationDurationMs = 30000;
constexpr uint16_t kMicCalibrationLogPeriodMs = 500;
constexpr bool kULockRequireKeyToStartDetection = true;
constexpr uint32_t kLaUnlockHoldMs = 3000;
constexpr float kMicRmsForScreenFullScale = 180.0f;
constexpr bool kScreenEnableMicScope = true;

}  // namespace config
