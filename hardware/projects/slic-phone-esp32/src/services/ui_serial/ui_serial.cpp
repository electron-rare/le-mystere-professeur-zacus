#include "ui_serial.h"

#include <ArduinoJson.h>

#include <cstring>

namespace {

HardwareSerial* g_serial = nullptr;
UiSerialCommandHandler g_handler = nullptr;
void* g_handlerCtx = nullptr;

constexpr size_t kLineMax = 512U;
char g_line[kLineMax + 1U] = {};
size_t g_lineLen = 0U;
bool g_dropLine = false;
bool g_ready = false;

UiSerialAction parseAction(const char* token) {
  if (token == nullptr) {
    return UiSerialAction::kUnknown;
  }
  if (strcmp(token, "play_pause") == 0) {
    return UiSerialAction::kPlayPause;
  }
  if (strcmp(token, "next") == 0) {
    return UiSerialAction::kNext;
  }
  if (strcmp(token, "prev") == 0) {
    return UiSerialAction::kPrev;
  }
  if (strcmp(token, "vol_delta") == 0) {
    return UiSerialAction::kVolDelta;
  }
  if (strcmp(token, "vol_set") == 0) {
    return UiSerialAction::kVolSet;
  }
  if (strcmp(token, "source_set") == 0) {
    return UiSerialAction::kSourceSet;
  }
  if (strcmp(token, "seek") == 0) {
    return UiSerialAction::kSeek;
  }
  if (strcmp(token, "station_delta") == 0) {
    return UiSerialAction::kStationDelta;
  }
  if (strcmp(token, "request_state") == 0) {
    return UiSerialAction::kRequestState;
  }
  return UiSerialAction::kUnknown;
}

void processJsonLine(const char* line) {
  if (line == nullptr || line[0] == '\0' || g_handler == nullptr) {
    return;
  }

  StaticJsonDocument<640> doc;
  if (deserializeJson(doc, line) != DeserializationError::Ok) {
    return;
  }
  const char* type = doc["t"] | "";
  if (strcmp(type, "cmd") != 0) {
    return;
  }

  UiSerialCommand cmd;
  cmd.action = parseAction(doc["a"] | "");
  if (cmd.action == UiSerialAction::kUnknown) {
    return;
  }
  if (doc.containsKey("v")) {
    JsonVariantConst v = doc["v"];
    if (v.is<int32_t>() || v.is<int>() || v.is<long>()) {
      cmd.hasIntValue = true;
      cmd.intValue = v.as<int32_t>();
    } else if (v.is<const char*>()) {
      cmd.hasTextValue = true;
      snprintf(cmd.textValue, sizeof(cmd.textValue), "%s", v.as<const char*>());
    }
  }

  g_handler(cmd, g_handlerCtx);
}

void sendDoc(const JsonDocument& doc) {
  if (g_serial == nullptr) {
    return;
  }
  serializeJson(doc, *g_serial);
  g_serial->print('\n');
}

}  // namespace

void uiSerialInit(HardwareSerial& serial, uint32_t baud, int8_t rxPin, int8_t txPin) {
  g_serial = &serial;
  g_serial->begin(baud, SERIAL_8N1, rxPin, txPin);
  g_lineLen = 0U;
  g_dropLine = false;
  g_ready = true;
}

void uiSerialSetCommandHandler(UiSerialCommandHandler handler, void* ctx) {
  g_handler = handler;
  g_handlerCtx = ctx;
}

bool uiSerialIsReady() {
  return g_ready && g_serial != nullptr;
}

void uiSerialPoll(uint32_t nowMs) {
  (void)nowMs;
  if (!uiSerialIsReady()) {
    return;
  }
  while (g_serial->available() > 0) {
    const int raw = g_serial->read();
    if (raw < 0) {
      break;
    }
    const char c = static_cast<char>(raw);
    if (c == '\r') {
      continue;
    }
    if (c == '\n') {
      if (!g_dropLine) {
        g_line[g_lineLen] = '\0';
        processJsonLine(g_line);
      }
      g_lineLen = 0U;
      g_dropLine = false;
      continue;
    }
    if (g_dropLine) {
      continue;
    }
    if (g_lineLen >= kLineMax) {
      g_dropLine = true;
      g_lineLen = 0U;
      continue;
    }
    g_line[g_lineLen++] = c;
  }
}

void uiSerialPublishState(const UiSerialState& state) {
  if (!uiSerialIsReady()) {
    return;
  }
  StaticJsonDocument<512> doc;
  doc["t"] = "state";
  doc["playing"] = state.playing;
  doc["source"] = state.source != nullptr ? state.source : "sd";
  doc["title"] = state.title != nullptr ? state.title : "";
  doc["artist"] = state.artist != nullptr ? state.artist : "";
  doc["station"] = state.station != nullptr ? state.station : "";
  doc["pos"] = state.pos;
  doc["dur"] = state.dur;
  doc["vol"] = state.vol;
  doc["rssi"] = state.rssi;
  doc["buffer"] = state.buffer;
  doc["error"] = state.error != nullptr ? state.error : "";
  sendDoc(doc);
}

void uiSerialPublishTick(const UiSerialTick& tick) {
  if (!uiSerialIsReady()) {
    return;
  }
  StaticJsonDocument<160> doc;
  doc["t"] = "tick";
  doc["pos"] = tick.pos;
  doc["buffer"] = tick.buffer;
  doc["vu"] = tick.vu;
  sendDoc(doc);
}

void uiSerialPublishHeartbeat(uint32_t nowMs) {
  if (!uiSerialIsReady()) {
    return;
  }
  StaticJsonDocument<64> doc;
  doc["t"] = "hb";
  doc["ms"] = nowMs;
  sendDoc(doc);
}

void uiSerialPublishList(const UiSerialList& list) {
  if (!uiSerialIsReady()) {
    return;
  }
  StaticJsonDocument<512> doc;
  doc["t"] = "list";
  doc["source"] = list.source != nullptr ? list.source : "sd";
  doc["offset"] = list.offset;
  doc["total"] = list.total;
  doc["cursor"] = list.cursor;
  JsonArray items = doc.createNestedArray("items");
  for (uint8_t i = 0U; i < list.count && i < 8U; ++i) {
    const char* item = list.items[i] != nullptr ? list.items[i] : "";
    items.add(item);
  }
  sendDoc(doc);
}
