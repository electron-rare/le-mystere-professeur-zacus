#include "runtime_state.h"

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
                    config::kScreenUpdatePeriodMs,
                    config::kScreenChangeMinPeriodMs);
Mp3Player g_mp3(config::kPinI2SBclk,
                config::kPinI2SLrc,
                config::kPinI2SDout,
                config::kMp3Path,
                config::kPinAudioPaEnable);
I2sJinglePlayer g_unlockJinglePlayer(config::kPinI2SBclk,
                                     config::kPinI2SLrc,
                                     config::kPinI2SDout,
                                     config::kI2sOutputPort);
AsyncAudioService g_asyncAudio(config::kPinI2SBclk,
                               config::kPinI2SLrc,
                               config::kPinI2SDout,
                               config::kI2sOutputPort,
                               config::kBootRadioScanChunkMs);

RuntimeMode g_mode = RuntimeMode::kSignal;
bool g_laDetectionEnabled = true;
bool g_uSonFunctional = false;
bool g_uLockListening = false;
uint32_t g_laHoldAccumMs = 0;
uint32_t g_lastLoopMs = 0;
bool g_paEnableActiveHigh = config::kPinAudioPaEnableActiveHigh;
bool g_paEnabledRequest = true;
bool g_littleFsReady = false;

UnlockJingleState g_unlockJingle;
BootAudioProtocolState g_bootAudioProtocol;
KeyTuneState g_keyTune;
KeySelfTestState g_keySelfTest;
MicCalibrationState g_micCalibration;
Mp3FormatTestState g_mp3FormatTest;
