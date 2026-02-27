#ifndef VISUAL_SCOPE_DISPLAY_H
#define VISUAL_SCOPE_DISPLAY_H

#include <Arduino.h>

class ScopeDisplay {
public:
    ScopeDisplay();

    bool begin();
    void end();
    bool supported() const;
    bool enabled() const;
    bool configure(uint16_t frequency_hz, uint8_t amplitude);
    void enable(bool value);
    void tick();

    uint16_t frequency() const;
    uint8_t amplitude() const;

private:
    static constexpr uint32_t kTickIntervalUs = 300;
    static constexpr float kTau = 6.283185307179586f;

    bool initialized_;
    bool configured_;
    bool enabled_;
    bool supported_;
    uint16_t frequency_hz_;
    uint8_t amplitude_;
    uint32_t last_tick_us_;
    float phase_;
};

#endif  // VISUAL_SCOPE_DISPLAY_H
