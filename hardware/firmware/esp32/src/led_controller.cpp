#include "led_controller.h"

LedController::LedController(uint8_t pinR, uint8_t pinG, uint8_t pinB)
    : pinR_(pinR), pinG_(pinG), pinB_(pinB) {}

void LedController::begin() {
  pinMode(pinR_, OUTPUT);
  pinMode(pinG_, OUTPUT);
  pinMode(pinB_, OUTPUT);
  setColor(false, false, false);
}

void LedController::showLaDetected() {
  setColor(false, true, false);
}

void LedController::showMp3Playing() {
  setColor(false, false, true);
}

void LedController::showMp3Paused() {
  setColor(true, false, false);
}

void LedController::updateRandom(uint32_t nowMs) {
  if (nowMs < nextUpdateMs_) {
    return;
  }

  switch (random(0, 5)) {
    case 0:
      setColor(true, false, false);
      break;
    case 1:
      setColor(false, false, true);
      break;
    case 2:
      setColor(true, false, true);
      break;
    case 3:
      setColor(true, true, false);
      break;
    default:
      setColor(false, false, false);
      break;
  }

  nextUpdateMs_ = nowMs + random(120, 500);
}

void LedController::setColor(bool r, bool g, bool b) {
  digitalWrite(pinR_, r ? HIGH : LOW);
  digitalWrite(pinG_, g ? HIGH : LOW);
  digitalWrite(pinB_, b ? HIGH : LOW);
}
