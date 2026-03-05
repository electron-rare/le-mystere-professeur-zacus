#include "codec_es8388_driver.h"

#include <Wire.h>

namespace {

bool isI2cAddressReachable(uint8_t address) {
  Wire.beginTransmission(address);
  return Wire.endTransmission() == 0;
}

}  // namespace

CodecEs8388Driver::CodecEs8388Driver(uint8_t i2cSdaPin,
                                     uint8_t i2cSclPin,
                                     uint32_t i2cClockHz,
                                     uint8_t preferredI2cAddress,
                                     uint8_t i2sBclkPin,
                                     uint8_t i2sWsPin,
                                     uint8_t i2sDoutPin,
                                     uint8_t i2sDinPin,
                                     uint8_t i2sPort,
                                     int8_t paEnablePin)
    : i2cSdaPin_(i2cSdaPin),
      i2cSclPin_(i2cSclPin),
      i2cClockHz_(i2cClockHz),
      preferredI2cAddress_(preferredI2cAddress),
      i2sBclkPin_(i2sBclkPin),
      i2sWsPin_(i2sWsPin),
      i2sDoutPin_(i2sDoutPin),
      i2sDinPin_(i2sDinPin),
      i2sPort_(i2sPort),
      paEnablePin_(paEnablePin),
      codecAddress_(preferredI2cAddress) {
  pins_.addI2C(PinFunction::CODEC,
               static_cast<int>(i2cSclPin_),
               static_cast<int>(i2cSdaPin_),
               -1,
               i2cClockHz_,
               DEFAULT_WIRE,
               true);
  pins_.addI2S(PinFunction::CODEC,
               0,
               static_cast<int>(i2sBclkPin_),
               static_cast<int>(i2sWsPin_),
               static_cast<int>(i2sDoutPin_),
               static_cast<int>(i2sDinPin_),
               static_cast<int>(i2sPort_));
  if (paEnablePin_ >= 0) {
    pins_.addPin(PinFunction::PA, static_cast<int>(paEnablePin_), PinLogic::Output);
  }
}

bool CodecEs8388Driver::begin(bool useLine2Input, uint8_t micGainDb) {
  useLine2Input_ = useLine2Input;
  micGainDb_ = clampMicGainDb(micGainDb);

  uint8_t detectedAddress = preferredI2cAddress_;
  if (!detectAddress(&detectedAddress)) {
    ready_ = false;
    return false;
  }

  codecAddress_ = detectedAddress;
  if (!initDriver()) {
    ready_ = false;
    return false;
  }
  ready_ = true;
  if (!applyInputConfig()) {
    ready_ = false;
    return false;
  }
  if (!setOutputVolumeRaw(kOutVol0dBRaw, true)) {
    ready_ = false;
    return false;
  }
  if (!driver_.setMute(false)) {
    ready_ = false;
    return false;
  }
  return true;
}

bool CodecEs8388Driver::ensureReady() {
  if (ready_) {
    return true;
  }
  return begin(useLine2Input_, micGainDb_);
}

bool CodecEs8388Driver::configureInput(bool useLine2Input, uint8_t micGainDb) {
  useLine2Input_ = useLine2Input;
  micGainDb_ = clampMicGainDb(micGainDb);
  if (!ensureReady()) {
    return false;
  }
  return applyInputConfig();
}

bool CodecEs8388Driver::isReady() const {
  return ready_;
}

uint8_t CodecEs8388Driver::address() const {
  return codecAddress_;
}

bool CodecEs8388Driver::readRegister(uint8_t reg, uint8_t* value) {
  if (value == nullptr || !ensureReady()) {
    return false;
  }
  return es8388_read_reg(reg, value) == RESULT_OK;
}

bool CodecEs8388Driver::writeRegister(uint8_t reg, uint8_t value) {
  if (!ensureReady()) {
    return false;
  }
  return es8388_write_reg(reg, value) == RESULT_OK;
}

bool CodecEs8388Driver::setOutputVolumeRaw(uint8_t rawValue, bool includeOut2) {
  if (!ensureReady()) {
    return false;
  }
  if (rawValue > kOutVolMaxRaw) {
    rawValue = kOutVolMaxRaw;
  }

  bool ok = true;
  ok = ok && (es8388_write_reg(ES8388_DACCONTROL24, rawValue) == RESULT_OK);
  ok = ok && (es8388_write_reg(ES8388_DACCONTROL25, rawValue) == RESULT_OK);
  if (includeOut2) {
    ok = ok && (es8388_write_reg(ES8388_DACCONTROL26, rawValue) == RESULT_OK);
    ok = ok && (es8388_write_reg(ES8388_DACCONTROL27, rawValue) == RESULT_OK);
  }
  return ok;
}

bool CodecEs8388Driver::setOutputVolumePercent(uint8_t percent, bool includeOut2) {
  return setOutputVolumeRaw(outputRawFromPercent(percent), includeOut2);
}

bool CodecEs8388Driver::setMute(bool mute) {
  if (!ensureReady()) {
    return false;
  }
  return driver_.setMute(mute);
}

uint8_t CodecEs8388Driver::outputRawFromPercent(uint8_t percent) {
  if (percent > 100U) {
    percent = 100U;
  }
  return static_cast<uint8_t>((static_cast<uint16_t>(percent) * kOutVolMaxRaw) / 100U);
}

bool CodecEs8388Driver::detectAddress(uint8_t* outAddress) {
  if (outAddress == nullptr) {
    return false;
  }

  Wire.begin(i2cSdaPin_, i2cSclPin_, i2cClockHz_);
  uint8_t candidate = preferredI2cAddress_;
  if (isI2cAddressReachable(candidate)) {
    *outAddress = candidate;
    return true;
  }

  candidate = (preferredI2cAddress_ == 0x10U) ? 0x11U : 0x10U;
  if (isI2cAddressReachable(candidate)) {
    *outAddress = candidate;
    return true;
  }

  return false;
}

bool CodecEs8388Driver::initDriver() {
  if (ready_) {
    driver_.end();
    ready_ = false;
  }

  codecConfig_ = audio_driver::CodecConfig();
  codecConfig_.input_device = useLine2Input_ ? ADC_INPUT_LINE2 : ADC_INPUT_LINE1;
  codecConfig_.output_device = DAC_OUTPUT_ALL;
  codecConfig_.i2s.mode = MODE_SLAVE;
  codecConfig_.i2s.fmt = I2S_NORMAL;
  codecConfig_.i2s.bits = BIT_LENGTH_16BITS;
  codecConfig_.i2s.channels = CHANNELS2;
  codecConfig_.i2s.rate = RATE_44K;
  codecConfig_.sd_active = false;
  codecConfig_.sdmmc_active = false;

  auto i2cPins = pins_.getI2CPins(PinFunction::CODEC);
  if (i2cPins) {
    auto value = i2cPins.value();
    value.address = codecAddress_;
    pins_.setI2C(value);
  }
  return driver_.begin(codecConfig_, pins_);
}

bool CodecEs8388Driver::applyInputConfig() {
  const es8388_input_device_t inputDevice =
      useLine2Input_ ? ESP8388_INPUT_LINPUT2_RINPUT2 : ESP8388_INPUT_LINPUT1_RINPUT1;
  if (es8388_config_input_device(inputDevice) != RESULT_OK) {
    return false;
  }
  return es8388_set_mic_gain(mapMicGain(micGainDb_)) == RESULT_OK;
}

uint8_t CodecEs8388Driver::clampMicGainDb(uint8_t micGainDb) {
  if (micGainDb > 24U) {
    micGainDb = 24U;
  }
  return static_cast<uint8_t>((micGainDb / 3U) * 3U);
}

es_mic_gain_t CodecEs8388Driver::mapMicGain(uint8_t micGainDb) {
  switch (clampMicGainDb(micGainDb)) {
    case 0:
      return MIC_GAIN_0DB;
    case 3:
      return MIC_GAIN_3DB;
    case 6:
      return MIC_GAIN_6DB;
    case 9:
      return MIC_GAIN_9DB;
    case 12:
      return MIC_GAIN_12DB;
    case 15:
      return MIC_GAIN_15DB;
    case 18:
      return MIC_GAIN_18DB;
    case 21:
      return MIC_GAIN_21DB;
    case 24:
    default:
      return MIC_GAIN_24DB;
  }
}
