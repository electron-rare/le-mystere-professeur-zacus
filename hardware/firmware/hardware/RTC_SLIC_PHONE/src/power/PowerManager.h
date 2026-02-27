// PowerManager.h
// Gestion batterie, deep sleep, wakeup

#ifndef POWERMANAGER_H
#define POWERMANAGER_H

#include <Arduino.h>

class PowerManager {
public:
    PowerManager();
    void monitorBattery(uint8_t pin);
    void enterDeepSleep(uint32_t ms);
    void wakeupOnPin(uint8_t pin);
    float getBatteryVoltage(uint8_t pin);
};

#endif // POWERMANAGER_H
