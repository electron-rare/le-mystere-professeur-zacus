#pragma once

#include <Arduino.h>

#include "ui_link_v2.h"

class UiLinkClient {
 public:
  using FrameHandler = void (*)(const UiLinkFrame& frame, uint32_t nowMs, void* ctx);

  void begin(HardwareSerial& serial, uint32_t baud);
  void setFrameHandler(FrameHandler handler, void* ctx);
  void poll(uint32_t nowMs);

  bool sendHello(const char* uiType, const char* uiId, const char* fw, const char* caps);
  bool sendPong(uint32_t nowMs);
  bool sendButton(UiBtnId id, UiBtnAction action, uint32_t nowMs);

  bool connected() const;
  uint32_t lastRxMs() const;

 private:
  bool sendFrame(const char* type, const UiLinkField* fields, uint8_t fieldCount);

  HardwareSerial* serial_ = nullptr;
  FrameHandler frameHandler_ = nullptr;
  void* frameHandlerCtx_ = nullptr;

  char lineBuf_[UILINK_V2_MAX_LINE + 1U] = {};
  size_t lineLen_ = 0U;
  bool dropLine_ = false;

  bool connected_ = false;
  uint32_t lastRxMs_ = 0U;
};
