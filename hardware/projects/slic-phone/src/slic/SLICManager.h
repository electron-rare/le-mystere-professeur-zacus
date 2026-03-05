#ifndef SLIC_MANAGER_H
#define SLIC_MANAGER_H

#include <Arduino.h>

#include "slic/SlicController.h"

enum class SLICLineState : uint8_t {
    UNINITIALIZED = 0,
    ON_HOOK,
    OFF_HOOK,
    RINGING
};

class SLICManager {
public:
    explicit SLICManager(SlicController* controller = nullptr);
    void attachController(SlicController* controller);
    void begin();
    bool begin(const SlicPins& pins);
    void monitorLine();
    void controlCall();
    void controlCall(bool incoming_ring);
    SLICLineState state() const;
    bool isHookOff() const;

private:
    SlicController* controller_;
    SLICLineState state_;
    bool incoming_ring_;
};

#endif  // SLIC_MANAGER_H
