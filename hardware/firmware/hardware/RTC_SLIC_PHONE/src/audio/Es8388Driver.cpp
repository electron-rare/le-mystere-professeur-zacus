#include "audio/Es8388Driver.h"

#include <Wire.h>

#include <algorithm>
#include <cmath>
#include <initializer_list>

namespace {
constexpr float kEs8388VolumeDbMin = -96.0f;
constexpr float kEs8388VolumeDbMax = 0.0f;

float percentToVolumeDb(uint8_t percent) {
    const float clamped = static_cast<float>(std::min<uint8_t>(100U, percent));
    const float normalized = clamped / 100.0f;
    // Linear in dB (perceptually logarithmic gain); 0%= -96 dB, 100%=0 dB.
    return kEs8388VolumeDbMin + (kEs8388VolumeDbMax - kEs8388VolumeDbMin) * normalized;
}

uint8_t dbToVolumeReg(float db) {
    const float clamped_db = std::max(kEs8388VolumeDbMin, std::min(kEs8388VolumeDbMax, db));
    // ES8388: 0x00 = 0 dB, 0xC0 = -96 dB (0.5 dB/step).
    return static_cast<uint8_t>(std::lround((-clamped_db) * 2.0f));
}

constexpr uint8_t kEs8388DacUnmuted = 0x32;      // DACCONTROL3 unmute (spec baseline)
constexpr uint8_t kEs8388DacMuted = 0x36;        // DACCONTROL3 mute (spec example)
constexpr uint8_t kEs8388DacRoute = 0xB8;        // DAC->mixer baseline path
constexpr uint8_t kEs8388Output0dB = 0x1E;       // LOUT/ROUT driver volume 0dB
}  // namespace

bool Es8388Driver::begin(int sda_pin, int scl_pin, uint8_t address) {
    address_ = address;
    Wire.begin(sda_pin, scl_pin);
    Wire.setClock(100000);

    const auto write_sequence = [&](std::initializer_list<std::pair<uint8_t, uint8_t>> seq) {
        bool ok = true;
        for (const auto& it : seq) {
            ok &= writeReg(it.first, it.second);
        }
        return ok;
    };

    // ES8388 setup aligned with A1S board spec:
    // - I2C 100 kHz on SDA=33/SCL=32
    // - full-duplex I2S slave mode
    // - 16-bit, ratio 256
    // - conservative output driver values (0x1E = 0 dB)
    const bool ok = write_sequence(
        {
            {0x19, 0x04},  // DACCONTROL3: mute during init.
            {0x01, 0x50},  // CONTROL2
            {0x02, 0x00},  // CHIPPOWER normal mode
            {0x35, 0xA0},  // Disable internal DLL for low sample rates stability.
            {0x37, 0xD0},
            {0x39, 0xD0},
            {0x08, 0x00},  // MASTERMODE: codec slave
            {0x04, 0xC0},  // DACPOWER: DAC outputs disabled while configuring
            {0x00, 0x12},  // CONTROL1: play+record mode
            {0x17, 0x18},  // DACCONTROL1: 16-bit I2S
            {0x18, 0x02},  // DACCONTROL2: single speed, ratio 256
            {0x26, 0x00},  // DACCONTROL16: DAC to mixer
            {0x27, kEs8388DacRoute},  // DACCONTROL17: DAC -> mixer path (spec baseline 0xB8)
            {0x2A, kEs8388DacRoute},  // DACCONTROL20: DAC -> mixer path (spec baseline 0xB8)
            {0x2B, 0x80},  // DACCONTROL21
            {0x2D, 0x00},  // DACCONTROL23
            {0x2E, kEs8388Output0dB},  // DACCONTROL24: LOUT1 volume = 0dB
            {0x2F, kEs8388Output0dB},  // DACCONTROL25: ROUT1 volume = 0dB
            {0x30, 0x00},  // DACCONTROL26
            {0x31, 0x00},  // DACCONTROL27
            {0x04, 0x3C},  // DACPOWER: enable LOUT/ROUT
            {0x03, 0xFF},  // ADCPOWER: power down before ADC config
            {0x09, 0xBB},  // ADCCONTROL1: PGA gain defaults
            {0x0A, 0x00},  // ADCCONTROL2: LIN1/RIN1
            {0x0B, 0x02},  // ADCCONTROL3
            {0x0C, 0x0C},  // ADCCONTROL4: 16-bit I2S
            {0x0D, 0x02},  // ADCCONTROL5: single speed, ratio 256
            {0x10, 0x00},  // ADCCONTROL8: 0 dB
            {0x11, 0x00},  // ADCCONTROL9: 0 dB
            {0x03, 0x09},  // ADCPOWER: enable ADC path
        });

    ready_ = ok;
    if (!ready_) {
        return false;
    }

    setMute(true);
    setVolume(volume_);
    setMute(muted_);
    setRoute(route_);
    return true;
}

bool Es8388Driver::setVolume(uint8_t percent) {
    volume_ = std::min<uint8_t>(100, percent);
    if (!ready_) {
        return false;
    }
    const float db = percentToVolumeDb(volume_);
    const uint8_t reg = dbToVolumeReg(db);
    // DAC digital volume controls.
    return writeReg(0x1A, reg) && writeReg(0x1B, reg);
}

bool Es8388Driver::setMute(bool enabled) {
    muted_ = enabled;
    if (!ready_) {
        return false;
    }
    // DACCONTROL3 bit2 is mute; use spec baseline values.
    return writeReg(0x19, static_cast<uint8_t>(enabled ? kEs8388DacMuted : kEs8388DacUnmuted));
}

bool Es8388Driver::setRoute(const String& route) {
    route_ = route;
    route_.toLowerCase();
    if (!ready_) {
        return false;
    }

    // Keep route as metadata and ensure output path is enabled for RTC.
    if (route_ == "rtc") {
        return writeReg(0x26, 0x00) && writeReg(0x27, kEs8388DacRoute) && writeReg(0x2A, kEs8388DacRoute) &&
               writeReg(0x04, 0x3C);
    }
    if (route_ == "none") {
        return writeReg(0x04, 0xC0);
    }
    route_ = "rtc";
    return writeReg(0x26, 0x00) && writeReg(0x27, kEs8388DacRoute) && writeReg(0x2A, kEs8388DacRoute) &&
           writeReg(0x04, 0x3C);
}

bool Es8388Driver::isReady() const {
    return ready_;
}

uint8_t Es8388Driver::volume() const {
    return volume_;
}

bool Es8388Driver::muted() const {
    return muted_;
}

String Es8388Driver::route() const {
    return route_;
}

bool Es8388Driver::writeReg(uint8_t reg, uint8_t value) {
    Wire.beginTransmission(address_);
    Wire.write(reg);
    Wire.write(value);
    return Wire.endTransmission() == 0;
}
