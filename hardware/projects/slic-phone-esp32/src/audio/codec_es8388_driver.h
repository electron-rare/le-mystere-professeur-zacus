#pragma once

#include <Arduino.h>

#include <Driver.h>
#include <Driver/es8388/es8388.h>

class CodecEs8388Driver {
 public:
  CodecEs8388Driver(uint8_t i2cSdaPin,
                    uint8_t i2cSclPin,
                    uint32_t i2cClockHz,
                    uint8_t preferredI2cAddress,
                    uint8_t i2sBclkPin,
                    uint8_t i2sWsPin,
                    uint8_t i2sDoutPin,
                    uint8_t i2sDinPin,
                    uint8_t i2sPort,
                    int8_t paEnablePin);

  bool begin(bool useLine2Input, uint8_t micGainDb);
  bool ensureReady();
  bool configureInput(bool useLine2Input, uint8_t micGainDb);

  bool isReady() const;
  uint8_t address() const;

  bool readRegister(uint8_t reg, uint8_t* value);
  bool writeRegister(uint8_t reg, uint8_t value);
  bool setOutputVolumeRaw(uint8_t rawValue, bool includeOut2);
  bool setOutputVolumePercent(uint8_t percent, bool includeOut2);
  bool setMute(bool mute);

  static uint8_t outputRawFromPercent(uint8_t percent);

 private:
  static constexpr uint8_t kOutVolMaxRaw = 0x21;
  static constexpr uint8_t kOutVol0dBRaw = 0x1E;

  bool detectAddress(uint8_t* outAddress);
  bool initDriver();
  bool applyInputConfig();
  static uint8_t clampMicGainDb(uint8_t micGainDb);
  static es_mic_gain_t mapMicGain(uint8_t micGainDb);

  uint8_t i2cSdaPin_;
  uint8_t i2cSclPin_;
  uint32_t i2cClockHz_;
  uint8_t preferredI2cAddress_;
  uint8_t i2sBclkPin_;
  uint8_t i2sWsPin_;
  uint8_t i2sDoutPin_;
  uint8_t i2sDinPin_;
  uint8_t i2sPort_;
  int8_t paEnablePin_;

  bool ready_ = false;
  bool useLine2Input_ = false;
  uint8_t micGainDb_ = 24;
  uint8_t codecAddress_ = 0x10;

  audio_driver::DriverPins pins_;
  audio_driver::AudioDriverES8388Class driver_{1};
  audio_driver::CodecConfig codecConfig_;
};
