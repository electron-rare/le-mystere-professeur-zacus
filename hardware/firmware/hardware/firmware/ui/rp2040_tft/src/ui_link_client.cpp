#include "ui_link_client.h"

#include <cstdio>

namespace {

void setField(UiLinkField* field, const char* key, const char* value) {
  if (field == nullptr) {
    return;
  }
  snprintf(field->key, sizeof(field->key), "%s", key != nullptr ? key : "");
  snprintf(field->value, sizeof(field->value), "%s", value != nullptr ? value : "");
}

const char* buttonIdToken(UiBtnId id) {
  switch (id) {
    case UI_BTN_OK:
      return "OK";
    case UI_BTN_NEXT:
      return "NEXT";
    case UI_BTN_PREV:
      return "PREV";
    case UI_BTN_BACK:
      return "BACK";
    case UI_BTN_VOL_UP:
      return "VOL_UP";
    case UI_BTN_VOL_DOWN:
      return "VOL_DOWN";
    case UI_BTN_MODE:
      return "MODE";
    case UI_BTN_UNKNOWN:
    default:
      return "UNKNOWN";
  }
}

const char* buttonActionToken(UiBtnAction action) {
  switch (action) {
    case UI_BTN_ACTION_DOWN:
      return "down";
    case UI_BTN_ACTION_UP:
      return "up";
    case UI_BTN_ACTION_CLICK:
      return "click";
    case UI_BTN_ACTION_LONG:
      return "long";
    case UI_BTN_ACTION_UNKNOWN:
    default:
      return "click";
  }
}

}  // namespace

void UiLinkClient::begin(HardwareSerial& serial, uint32_t baud) {
  serial_ = &serial;
  serial_->begin(baud);
  lineLen_ = 0U;
  dropLine_ = false;
  connected_ = false;
  lastRxMs_ = 0U;
}

void UiLinkClient::setFrameHandler(FrameHandler handler, void* ctx) {
  frameHandler_ = handler;
  frameHandlerCtx_ = ctx;
}

bool UiLinkClient::sendFrame(const char* type, const UiLinkField* fields, uint8_t fieldCount) {
  if (serial_ == nullptr || type == nullptr) {
    return false;
  }
  char line[UILINK_V2_MAX_LINE + 1U] = {};
  const size_t lineLen = uiLinkBuildLine(line, sizeof(line), type, fields, fieldCount);
  if (lineLen == 0U) {
    return false;
  }
  serial_->write(reinterpret_cast<const uint8_t*>(line), lineLen);
  return true;
}

bool UiLinkClient::sendHello(const char* uiType, const char* uiId, const char* fw, const char* caps) {
  UiLinkField fields[5] = {};
  setField(&fields[0], "proto", "2");
  setField(&fields[1], "ui_type", uiType);
  setField(&fields[2], "ui_id", uiId);
  setField(&fields[3], "fw", fw);
  setField(&fields[4], "caps", caps);
  return sendFrame("HELLO", fields, 5U);
}

bool UiLinkClient::sendPong(uint32_t nowMs) {
  UiLinkField fields[1] = {};
  char ts[20] = {};
  snprintf(ts, sizeof(ts), "%lu", static_cast<unsigned long>(nowMs));
  setField(&fields[0], "ms", ts);
  return sendFrame("PONG", fields, 1U);
}

bool UiLinkClient::sendButton(UiBtnId id, UiBtnAction action, uint32_t nowMs) {
  UiLinkField fields[3] = {};
  setField(&fields[0], "id", buttonIdToken(id));
  setField(&fields[1], "action", buttonActionToken(action));
  char ts[20] = {};
  snprintf(ts, sizeof(ts), "%lu", static_cast<unsigned long>(nowMs));
  setField(&fields[2], "ts", ts);
  return sendFrame("BTN", fields, 3U);
}

void UiLinkClient::poll(uint32_t nowMs) {
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
      if (!dropLine_ && lineLen_ > 0U) {
        lineBuf_[lineLen_] = '\0';
        UiLinkFrame frame = {};
        if (uiLinkParseLine(lineBuf_, &frame)) {
          lastRxMs_ = nowMs;
          if (frame.type == UILINK_MSG_ACK) {
            connected_ = true;
          } else if (frame.type == UILINK_MSG_PING) {
            sendPong(nowMs);
          }
          if (frameHandler_ != nullptr) {
            frameHandler_(frame, nowMs, frameHandlerCtx_);
          }
        }
      }
      lineLen_ = 0U;
      dropLine_ = false;
      continue;
    }

    if (dropLine_) {
      continue;
    }
    if (lineLen_ >= UILINK_V2_MAX_LINE) {
      lineLen_ = 0U;
      dropLine_ = true;
      continue;
    }
    lineBuf_[lineLen_++] = c;
  }

  if (lastRxMs_ > 0U && static_cast<uint32_t>(nowMs - lastRxMs_) > UILINK_V2_TIMEOUT_MS) {
    connected_ = false;
  }
}

bool UiLinkClient::connected() const {
  return connected_;
}

uint32_t UiLinkClient::lastRxMs() const {
  return lastRxMs_;
}
