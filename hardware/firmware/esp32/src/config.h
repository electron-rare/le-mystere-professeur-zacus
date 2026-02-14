#pragma once

#include <Arduino.h>

#ifndef USON_STORY_V2_DEFAULT
#define USON_STORY_V2_DEFAULT 1
#endif

#ifndef UI_SERIAL_ENABLED
#define UI_SERIAL_ENABLED 0
#endif

#ifndef UI_SERIAL_BAUD
#define UI_SERIAL_BAUD 115200
#endif

#ifndef UI_SERIAL_RX_PIN
#define UI_SERIAL_RX_PIN 18
#endif

#ifndef UI_SERIAL_TX_PIN
#define UI_SERIAL_TX_PIN 19
#endif

namespace config {

constexpr uint8_t kPinLedR = 16;
constexpr uint8_t kPinLedG = 17;
constexpr uint8_t kPinLedB = 4;
constexpr bool kDisableBoardRgbLeds = true;

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

// Mode I2S-only sur A252: DAC desactive.
// Le DAC hardware ESP32 existe seulement sur GPIO25/26 et ne doit pas etre utilise ici.
constexpr bool kEnableSineDac = false;
constexpr uint8_t kPinDacSine = 0xFF;

// Profil ESP32 Audio Kit V2.2 A252 (codec + SD_MMC)
constexpr uint8_t kPinI2SBclk = 27;
constexpr uint8_t kPinI2SLrc = 25;
constexpr uint8_t kPinI2SDout = 26;
constexpr int8_t kPinAudioPaEnable = 21;
constexpr bool kPinAudioPaEnableActiveHigh = true;
constexpr bool kBootAudioPaTogglePulse = true;
constexpr uint16_t kBootAudioPaToggleMs = 20;
constexpr uint8_t kI2sOutputPort = 0;
constexpr bool kEnableUnlockI2sJingle = true;
constexpr float kUnlockI2sJingleGain = 0.22f;
constexpr bool kEnableBootI2sNoiseFx = true;
constexpr uint16_t kBootI2sNoiseDurationMs = 1100;
constexpr uint16_t kBootI2sNoiseSampleRateHz = 22050;
constexpr uint16_t kBootI2sNoiseAttackMs = 90;
constexpr uint16_t kBootI2sNoiseReleaseMs = 260;
constexpr float kBootI2sNoiseGain = 0.18f;
constexpr bool kEnableBootAudioValidationProtocol = true;
constexpr uint32_t kBootAudioValidationTimeoutMs = 0;  // 0 => pas de timeout auto
constexpr uint8_t kBootAudioValidationMaxReplays = 6;
constexpr uint16_t kBootProtocolPromptPeriodMs = 3000;
constexpr uint16_t kBootRadioScanChunkMs = 18;
constexpr uint32_t kStoryEtape2DelayMs = 15UL * 60UL * 1000UL;
constexpr uint32_t kStoryEtape2TestDelayMs = 5000U;
constexpr bool kStoryV2EnabledDefault = (USON_STORY_V2_DEFAULT != 0);
constexpr bool kEnableInternalLittleFs = true;
constexpr bool kInternalLittleFsFormatOnFail = false;
constexpr bool kPreferLittleFsBootFx = true;
constexpr char kBootFxLittleFsPath[] = "/uson_boot_arcade_lowmono.mp3";
constexpr float kBootFxLittleFsGain = 0.24f;
constexpr uint32_t kBootFxLittleFsMaxDurationMs = 22000;

// ESP32 -> ESP8266 (ecran) en UART unidirectionnel
constexpr uint8_t kPinScreenTx = 22;
constexpr uint32_t kScreenBaud = 19200;
constexpr uint16_t kScreenUpdatePeriodMs = 250;
constexpr uint16_t kScreenChangeMinPeriodMs = 90;

// UART UI tactile externe (RP2040), module optionnel.
constexpr bool kUiSerialEnabled = (UI_SERIAL_ENABLED != 0);
constexpr uint32_t kUiSerialBaud = static_cast<uint32_t>(UI_SERIAL_BAUD);
constexpr int8_t kPinUiSerialRx = static_cast<int8_t>(UI_SERIAL_RX_PIN);
constexpr int8_t kPinUiSerialTx = static_cast<int8_t>(UI_SERIAL_TX_PIN);

// Clavier analogique (6 touches sur 1 entree ADC)
constexpr uint8_t kPinKeysAdc = 36;
constexpr uint16_t kKeysSampleEveryMs = 8;
constexpr uint16_t kKeysDebounceMs = 30;
// Profil A252 "dur" ajuste d'apres mesures live:
// ~1360 -> K4, ~1650 -> K5, ~1885 -> K6.
constexpr uint16_t kKeysReleaseThreshold = 3920;
constexpr uint16_t kKey1Max = 300;
constexpr uint16_t kKey2Max = 760;
constexpr uint16_t kKey3Max = 1340;
constexpr uint16_t kKey4Max = 1500;
constexpr uint16_t kKey5Max = 1770;
constexpr uint16_t kKey6Max = 2200;

constexpr char kMp3Path[] = "/track001.mp3";
constexpr bool kMp3FxOverlayModeDefault = false;  // false=DUCKING, true=OVERLAY
constexpr float kMp3FxDuckingGainDefault = 0.45f;
constexpr float kMp3FxOverlayGainDefault = 0.42f;
constexpr uint16_t kMp3FxDefaultDurationMs = 2200;

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
