#ifndef AUDIO_ES8388_DRIVER_H
#define AUDIO_ES8388_DRIVER_H

#include <Arduino.h>

#include "config/a1s_board_pins.h"

class Es8388Driver {
public:
    bool begin(int sda_pin, int scl_pin, uint8_t address = A1S_ES8388_I2C_ADDR);
    bool setVolume(uint8_t percent);
    bool setMute(bool enabled);
    bool setRoute(const String& route);

    bool isReady() const;
    uint8_t volume() const;
    bool muted() const;
    String route() const;

private:
    bool writeReg(uint8_t reg, uint8_t value);

    bool ready_ = false;
    uint8_t address_ = A1S_ES8388_I2C_ADDR;
    uint8_t volume_ = 60;
    bool muted_ = false;
    String route_ = "rtc";
};

#endif  // AUDIO_ES8388_DRIVER_H
