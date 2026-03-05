#ifndef KS0835_SLIC_CONTROLLER_H
#define KS0835_SLIC_CONTROLLER_H

#include "slic/SlicController.h"

class Ks0835SlicController : public SlicController {
public:
    Ks0835SlicController();
    bool begin(const SlicPins& pins) override;
    void setRing(bool enabled) override;
    void setLineEnabled(bool enabled) override;
    bool isHookOff() const override;
    void setPowerDown(bool enabled) override;
    bool isPowerDownEnabled() const override;
    void tick() override;

private:
    SlicPins pins_;
    bool initialized_;
    bool ring_enabled_;
    bool power_down_enabled_;
    bool fr_state_;
    uint32_t last_fr_toggle_ms_;
};

#endif  // KS0835_SLIC_CONTROLLER_H
