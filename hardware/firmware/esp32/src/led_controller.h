#pragma once

#include <Arduino.h>

class LedController {
 public:
  LedController(uint8_t pinR, uint8_t pinG, uint8_t pinB);

  void begin();
  void showLaDetected();
  void showMp3Playing();
  void showMp3Paused();
  void updateRandom(uint32_t nowMs);

 private:
  void setColor(bool r, bool g, bool b);

  uint8_t pinR_;
  uint8_t pinG_;
  uint8_t pinB_;
  uint32_t nextUpdateMs_ = 0;
};
