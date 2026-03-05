#include "SlicK50835F.h"

SlicK50835F::SlicK50835F(uint8_t pinHook, uint8_t pinRingCmd, uint8_t pinLineSense)
    : _pinHook(pinHook), _pinRingCmd(pinRingCmd), _pinLineSense(pinLineSense), _hookState(false), _lineState(false) {}

void SlicK50835F::begin() {
    pinMode(_pinHook, INPUT_PULLUP);
    pinMode(_pinRingCmd, OUTPUT);
    pinMode(_pinLineSense, INPUT);
    digitalWrite(_pinRingCmd, LOW);
}

void SlicK50835F::setRing(bool enable) {
    digitalWrite(_pinRingCmd, enable ? HIGH : LOW);
}

bool SlicK50835F::isHookOn() {
    _hookState = digitalRead(_pinHook) == LOW;
    return _hookState;
}

bool SlicK50835F::isLineActive() {
    _lineState = digitalRead(_pinLineSense) == HIGH;
    return _lineState;
}

void SlicK50835F::loop() {
    // Mettre à jour les états, ajouter logique avancée si besoin
    isHookOn();
    isLineActive();
}
