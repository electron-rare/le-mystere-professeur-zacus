#ifndef SLIC_CONTROLLER_H
#define SLIC_CONTROLLER_H

#include <Arduino.h>

struct SlicPins {
    uint8_t pin_rm;
    uint8_t pin_fr;
    uint8_t pin_shk;
    int8_t pin_line_enable;
    int8_t pin_pd;
    bool hook_active_high;
};

class SlicController {
public:
    virtual ~SlicController() = default;
    virtual bool begin(const SlicPins& pins) = 0;
    virtual void setRing(bool enabled) = 0;
    virtual void setLineEnabled(bool enabled) = 0;
    virtual bool isHookOff() const = 0;
    virtual void setPowerDown(bool enabled) = 0;
    virtual bool isPowerDownEnabled() const = 0;
    virtual void tick() = 0;
};

#endif  // SLIC_CONTROLLER_H
