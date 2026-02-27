// Classe d'abstraction pour le SLIC K50835F (AG1171S)
// Permet de piloter la ligne RTC, le hook, la sonnerie et la détection d'état

#ifndef SLIC_K50835F_H
#define SLIC_K50835F_H

#include <Arduino.h>

class SlicK50835F {
public:
    SlicK50835F(uint8_t pinHook, uint8_t pinRingCmd, uint8_t pinLineSense);
    void begin();
    void setRing(bool enable);
    bool isHookOn();
    bool isLineActive();
    void loop();

private:
    uint8_t _pinHook;
    uint8_t _pinRingCmd;
    uint8_t _pinLineSense;
    bool _hookState;
    bool _lineState;
};

#endif // SLIC_K50835F_H
