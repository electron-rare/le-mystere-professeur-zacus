#pragma once

#include <Arduino.h>

class BootProtocolController {
 public:
  struct Hooks {
    void (*start)(uint32_t nowMs) = nullptr;
    void (*update)(uint32_t nowMs) = nullptr;
    void (*onKey)(uint8_t key, uint32_t nowMs) = nullptr;
    bool (*isActive)() = nullptr;
  };

  explicit BootProtocolController(const Hooks& hooks);

  void start(uint32_t nowMs);
  void update(uint32_t nowMs);
  void onKey(uint8_t key, uint32_t nowMs);
  bool isActive() const;

 private:
  Hooks hooks_;
};
