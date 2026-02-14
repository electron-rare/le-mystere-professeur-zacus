#include "uart_link.h"

#include <cstring>

void UartLink::begin(HardwareSerial& serial, uint32_t baud, int8_t rxPin, int8_t txPin) {
  serial_ = &serial;
  (void)rxPin;
  (void)txPin;
  serial_->begin(baud);
  lineLen_ = 0U;
  droppingLine_ = false;
}

void UartLink::setJsonHandler(JsonHandler handler, void* ctx) {
  handler_ = handler;
  handlerCtx_ = ctx;
}

void UartLink::poll() {
  if (serial_ == nullptr) {
    return;
  }

  while (serial_->available() > 0) {
    const int raw = serial_->read();
    if (raw < 0) {
      break;
    }
    const char c = static_cast<char>(raw);
    if (c == '\r') {
      continue;
    }
    if (c == '\n') {
      if (!droppingLine_) {
        lineBuf_[lineLen_] = '\0';
        processLine(lineBuf_);
      }
      lineLen_ = 0U;
      droppingLine_ = false;
      continue;
    }
    if (droppingLine_) {
      continue;
    }
    if (lineLen_ >= kLineMax) {
      droppingLine_ = true;
      lineLen_ = 0U;
      continue;
    }
    lineBuf_[lineLen_++] = c;
  }
}

bool UartLink::processLine(const char* line) {
  if (line == nullptr || line[0] == '\0' || handler_ == nullptr) {
    return false;
  }

  StaticJsonDocument<640> doc;
  DeserializationError err = deserializeJson(doc, line);
  if (err) {
    return false;
  }
  handler_(doc, handlerCtx_);
  return true;
}

bool UartLink::sendRawLine(const char* line) {
  if (serial_ == nullptr || line == nullptr) {
    return false;
  }
  serial_->print(line);
  serial_->print('\n');
  return true;
}

bool UartLink::sendRequestState() {
  UiOutgoingCommand cmd;
  cmd.cmd = UiOutCmd::kRequestState;
  return sendCommand(cmd);
}

bool UartLink::sendCommand(const UiOutgoingCommand& command) {
  if (serial_ == nullptr || command.cmd == UiOutCmd::kNone) {
    return false;
  }

  StaticJsonDocument<192> doc;
  doc["t"] = "cmd";

  switch (command.cmd) {
    case UiOutCmd::kPlayPause:
      doc["a"] = "play_pause";
      break;
    case UiOutCmd::kNext:
      doc["a"] = "next";
      break;
    case UiOutCmd::kPrev:
      doc["a"] = "prev";
      break;
    case UiOutCmd::kVolDelta:
      doc["a"] = "vol_delta";
      doc["v"] = command.value;
      break;
    case UiOutCmd::kVolSet:
      doc["a"] = "vol_set";
      doc["v"] = command.value;
      break;
    case UiOutCmd::kSourceSet:
      doc["a"] = "source_set";
      doc["v"] = command.textValue;
      break;
    case UiOutCmd::kSeek:
      doc["a"] = "seek";
      doc["v"] = command.value;
      break;
    case UiOutCmd::kStationDelta:
      doc["a"] = "station_delta";
      doc["v"] = command.value;
      break;
    case UiOutCmd::kRequestState:
      doc["a"] = "request_state";
      break;
    case UiOutCmd::kNone:
    default:
      return false;
  }

  serializeJson(doc, *serial_);
  serial_->print('\n');
  return true;
}
