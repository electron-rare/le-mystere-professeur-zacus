#pragma once

#include <Arduino.h>
#include <driver/i2s.h>

#include "config.h"

class LaDetector {
 public:
  LaDetector(uint8_t micAdcPin,
             bool useI2sMic,
             uint8_t i2sBclkPin,
             uint8_t i2sWsPin,
             uint8_t i2sDinPin);

  void begin();
  void setCaptureEnabled(bool enabled);
  void update(uint32_t nowMs);
  bool isDetected() const;
  int8_t tuningOffset() const;
  uint8_t tuningConfidence() const;
  float targetRatio() const;
  float micMean() const;
  float micRms() const;
  uint16_t micMin() const;
  uint16_t micMax() const;
  uint16_t micPeakToPeak() const;

 private:
  bool beginI2sInput();
  void endI2sInput();
  void captureFromAdc();
  void captureFromI2s();
  bool beginCodec();
  bool configureCodecInput(bool useLine2);
  bool writeCodecReg(uint8_t reg, uint8_t value);
  bool isCodecAddressReachable(uint8_t address);
  void maybeAutoSwitchCodecInput(uint32_t nowMs);

  float goertzelPower(const int16_t* x, uint16_t n, float fs, float targetHz) const;
  bool detect(const int16_t* samples,
              float* targetRatio,
              int8_t* tuningOffset,
              uint8_t* tuningConfidence,
              float* micMean,
              float* micRms,
              uint16_t* micMin,
              uint16_t* micMax) const;

  uint8_t micAdcPin_;
  bool useI2sMic_;
  uint8_t i2sBclkPin_;
  uint8_t i2sWsPin_;
  uint8_t i2sDinPin_;
  i2s_port_t i2sPort_ = I2S_NUM_0;
  uint8_t codecAddress_ = config::kCodecI2CAddress;
  bool codecReady_ = false;
  bool codecUseLine2_ = config::kCodecMicUseLine2Input;
  bool codecAutoSwitched_ = false;
  uint32_t codecSilenceSinceMs_ = 0;
  bool i2sReady_ = false;
  bool captureEnabled_ = true;
  int16_t samples_[config::kDetectN] = {};
  uint16_t sampleIndex_ = 0;
  bool captureInProgress_ = false;
  uint32_t nextSampleUs_ = 0;
  uint32_t lastDetectMs_ = 0;
  bool detected_ = false;
  float targetRatio_ = 0.0f;
  int8_t tuningOffset_ = 0;
  uint8_t tuningConfidence_ = 0;
  float micMean_ = 0.0f;
  float micRms_ = 0.0f;
  uint16_t micMin_ = 0;
  uint16_t micMax_ = 0;
};
