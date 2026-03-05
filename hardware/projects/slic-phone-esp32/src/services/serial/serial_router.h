#pragma once

#include <Arduino.h>

struct SerialCommand {
  const char* line = nullptr;
  const char* token = nullptr;
  const char* args = nullptr;
};

class SerialRouter {
 public:
  using DispatchFn = void (*)(const SerialCommand& cmd, uint32_t nowMs, void* ctx);

  explicit SerialRouter(HardwareSerial& serial);

  void setDispatcher(DispatchFn dispatcher, void* ctx);
  void update(uint32_t nowMs);

 private:
  static void trim(char* line);
  static void extractToken(const char* line, char* outToken, size_t outTokenLen, const char** outArgs);

  HardwareSerial& serial_;
  DispatchFn dispatcher_ = nullptr;
  void* dispatcherCtx_ = nullptr;
  char buffer_[192] = {};
  char token_[64] = {};
  uint8_t len_ = 0;
};
