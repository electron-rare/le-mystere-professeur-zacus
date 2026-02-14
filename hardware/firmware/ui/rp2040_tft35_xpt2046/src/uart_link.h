#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

#include "../include/ui_protocol.h"

class UartLink {
 public:
  using JsonHandler = void (*)(const JsonDocument& doc, void* ctx);

  void begin(HardwareSerial& serial, uint32_t baud, int8_t rxPin, int8_t txPin);
  void setJsonHandler(JsonHandler handler, void* ctx);
  void poll();

  bool sendCommand(const UiOutgoingCommand& command);
  bool sendRequestState();
  bool sendRawLine(const char* line);

 private:
  bool processLine(const char* line);

  HardwareSerial* serial_ = nullptr;
  JsonHandler handler_ = nullptr;
  void* handlerCtx_ = nullptr;

  static constexpr size_t kLineMax = 512U;
  char lineBuf_[kLineMax + 1U] = {};
  size_t lineLen_ = 0U;
  bool droppingLine_ = false;
};
