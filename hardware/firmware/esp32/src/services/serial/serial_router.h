#pragma once

#include <Arduino.h>

class SerialRouter {
 public:
  using DispatchFn = void (*)(const char* cmd, uint32_t nowMs, void* ctx);

  explicit SerialRouter(HardwareSerial& serial);

  void setDispatcher(DispatchFn dispatcher, void* ctx);
  void update(uint32_t nowMs);

 private:
  static void normalize(char* line);

  HardwareSerial& serial_;
  DispatchFn dispatcher_ = nullptr;
  void* dispatcherCtx_ = nullptr;
  char buffer_[192] = {};
  uint8_t len_ = 0;
};
