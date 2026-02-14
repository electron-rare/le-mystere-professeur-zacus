#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>

#include "../include/ui_config.h"
#include "../include/ui_protocol.h"
#include "touch_calibration.h"
#include "uart_link.h"
#include "ui_renderer.h"
#include "ui_state.h"

namespace {

TFT_eSPI g_tft;
XPT2046_Touchscreen g_touch(ui_config::kPinTouchCs, ui_config::kPinTouchIrq);
TouchCalibration g_calibration;
UartLink g_uart;
UiStateModel g_ui;
UiRenderer g_renderer(g_tft);

UiRemoteState g_lastState;

struct TouchTracker {
  bool active = false;
  uint16_t startX = 0;
  uint16_t startY = 0;
  uint16_t lastX = 0;
  uint16_t lastY = 0;
  uint32_t startMs = 0;
  uint32_t lastActionMs = 0;
};

TouchTracker g_touchTracker;
uint32_t g_nextTouchPollMs = 0U;
uint32_t g_nextRenderMs = 0U;
bool g_forceRender = true;

template <typename T>
T clampValue(T value, T minValue, T maxValue) {
  if (value < minValue) {
    return minValue;
  }
  if (value > maxValue) {
    return maxValue;
  }
  return value;
}

bool readTouchPoint(uint16_t* outX, uint16_t* outY) {
  if (outX == nullptr || outY == nullptr) {
    return false;
  }
  if (ui_config::kPinTouchIrq >= 0 && digitalRead(static_cast<uint8_t>(ui_config::kPinTouchIrq)) == HIGH &&
      !g_touch.touched()) {
    return false;
  }
  if (!g_touch.touched()) {
    return false;
  }
  TS_Point p = g_touch.getPoint();
  if (p.z < 80) {
    return false;
  }
  return g_calibration.mapRaw(
      p.x, p.y, ui_config::kScreenWidth, ui_config::kScreenHeight, outX, outY);
}

void sendUiCommand(const UiOutgoingCommand& cmd) {
  if (cmd.cmd == UiOutCmd::kNone) {
    return;
  }
  g_uart.sendCommand(cmd);
}

void processGesture(uint16_t x, uint16_t y, uint32_t nowMs) {
  UiOutgoingCommand cmd;
  if (!g_touchTracker.active) {
    g_touchTracker.active = true;
    g_touchTracker.startX = x;
    g_touchTracker.startY = y;
    g_touchTracker.lastX = x;
    g_touchTracker.lastY = y;
    g_touchTracker.startMs = nowMs;
    return;
  }
  g_touchTracker.lastX = x;
  g_touchTracker.lastY = y;
}

void processTouch(uint32_t nowMs) {
  if (static_cast<int32_t>(nowMs - g_nextTouchPollMs) < 0) {
    return;
  }
  g_nextTouchPollMs = nowMs + ui_config::kTouchPollPeriodMs;

  uint16_t x = 0U;
  uint16_t y = 0U;
  const bool pressed = readTouchPoint(&x, &y);
  if (pressed) {
    processGesture(x, y, nowMs);
    return;
  }

  if (!g_touchTracker.active) {
    return;
  }

  const int16_t dx = static_cast<int16_t>(g_touchTracker.lastX) - static_cast<int16_t>(g_touchTracker.startX);
  const int16_t dy = static_cast<int16_t>(g_touchTracker.lastY) - static_cast<int16_t>(g_touchTracker.startY);
  const uint16_t adx = static_cast<uint16_t>(dx < 0 ? -dx : dx);
  const uint16_t ady = static_cast<uint16_t>(dy < 0 ? -dy : dy);
  const uint32_t pressMs = nowMs - g_touchTracker.startMs;

  if (static_cast<int32_t>(nowMs - g_touchTracker.lastActionMs) <
      static_cast<int32_t>(ui_config::kTouchDebounceMs)) {
    g_touchTracker.active = false;
    return;
  }

  UiOutgoingCommand cmd;
  bool send = false;
  if ((adx >= ui_config::kSwipeMinTravelPx || ady >= ui_config::kSwipeMinTravelPx) &&
      pressMs <= 900U) {
    send = g_ui.onSwipe(dx, dy, nowMs, &cmd);
  } else if (adx <= ui_config::kTapMaxTravelPx && ady <= ui_config::kTapMaxTravelPx &&
             pressMs <= ui_config::kGestureMaxTapMs) {
    send = g_ui.onTap(g_touchTracker.startX, g_touchTracker.startY, nowMs, &cmd);
  }

  if (send) {
    sendUiCommand(cmd);
    g_touchTracker.lastActionMs = nowMs;
  }
  g_touchTracker.active = false;
}

void handleIncomingJson(const JsonDocument& doc, void* ctx) {
  (void)ctx;
  const char* type = doc["t"] | "";
  const uint32_t nowMs = millis();
  if (strcmp(type, "state") == 0) {
    UiRemoteState state = g_lastState;
    if (doc.containsKey("playing")) {
      state.playing = doc["playing"] | false;
    }
    if (doc.containsKey("source")) {
      state.source = uiSourceFromToken(doc["source"] | "sd");
    }
    if (doc.containsKey("title")) {
      snprintf(state.title, sizeof(state.title), "%s", doc["title"] | "");
    }
    if (doc.containsKey("artist")) {
      snprintf(state.artist, sizeof(state.artist), "%s", doc["artist"] | "");
    }
    if (doc.containsKey("station")) {
      snprintf(state.station, sizeof(state.station), "%s", doc["station"] | "");
    }
    if (doc.containsKey("pos")) {
      state.posSec = doc["pos"] | state.posSec;
    }
    if (doc.containsKey("dur")) {
      state.durSec = doc["dur"] | state.durSec;
    }
    if (doc.containsKey("vol")) {
      state.volume = clampValue<int32_t>(doc["vol"] | state.volume, 0, 100);
    }
    if (doc.containsKey("rssi")) {
      state.rssi = doc["rssi"] | state.rssi;
    }
    if (doc.containsKey("buffer")) {
      state.bufferPercent = doc["buffer"] | state.bufferPercent;
    }
    if (doc.containsKey("error")) {
      snprintf(state.error, sizeof(state.error), "%s", doc["error"] | "");
    }
    g_lastState = state;
    g_ui.applyState(state, nowMs);
    return;
  }

  if (strcmp(type, "tick") == 0) {
    UiRemoteTick tick;
    tick.posSec = doc["pos"] | g_lastState.posSec;
    tick.bufferPercent = doc["buffer"] | g_lastState.bufferPercent;
    tick.vu = doc["vu"] | 0.0f;
    g_ui.applyTick(tick, nowMs);
    return;
  }

  if (strcmp(type, "hb") == 0) {
    g_ui.onHeartbeat(nowMs);
    return;
  }

  if (strcmp(type, "list") == 0) {
    UiRemoteList list;
    list.source = uiSourceFromToken(doc["source"] | "sd");
    list.offset = doc["offset"] | 0;
    list.total = doc["total"] | 0;
    list.cursor = doc["cursor"] | 0;
    JsonArrayConst arr = doc["items"].as<JsonArrayConst>();
    uint8_t idx = 0U;
    for (JsonVariantConst item : arr) {
      if (idx >= 8U) {
        break;
      }
      snprintf(list.items[idx], sizeof(list.items[idx]), "%s", item.as<const char*>());
      ++idx;
    }
    list.count = idx;
    g_ui.applyList(list, nowMs);
    return;
  }
}

void showCalibrationHintAndMaybeRun() {
  bool needCalibration = !g_calibration.load();
  if (!needCalibration) {
    g_renderer.drawBootScreen("Touch coin haut-gauche", "pour recalibrer");
    const uint32_t deadline = millis() + 1200U;
    while (static_cast<int32_t>(millis() - deadline) < 0) {
      uint16_t x = 0U;
      uint16_t y = 0U;
      if (readTouchPoint(&x, &y) && x < 80U && y < 80U) {
        needCalibration = true;
        break;
      }
      delay(20);
    }
  }
  if (needCalibration) {
    g_renderer.drawBootScreen("Calibration tactile", "Touchez les 3 points");
    if (!g_calibration.runWizard(g_tft, g_touch, ui_config::kScreenWidth, ui_config::kScreenHeight)) {
      g_renderer.drawBootScreen("Calibration echec", "Profil par defaut");
      TouchCalibration::Data* d = g_calibration.mutableData();
      d->valid = true;
      d->swapXY = false;
      d->invertX = false;
      d->invertY = false;
      d->xMin = 200;
      d->xMax = 3900;
      d->yMin = 200;
      d->yMax = 3900;
      g_calibration.save();
      delay(700);
    }
  }
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("[UI] RP2040 TFT touch UI boot");

  SPI.setSCK(ui_config::kPinSpiSck);
  SPI.setTX(ui_config::kPinSpiMosi);
  SPI.setRX(ui_config::kPinSpiMiso);

  g_tft.init();
  g_tft.setRotation(ui_config::kRotation);
  g_tft.fillScreen(TFT_BLACK);

  if (ui_config::kPinTouchIrq >= 0) {
    pinMode(static_cast<uint8_t>(ui_config::kPinTouchIrq), INPUT_PULLUP);
  }
  g_touch.begin();
  g_touch.setRotation(ui_config::kRotation);

  g_calibration.begin();
  g_renderer.begin();
  g_renderer.drawBootScreen("Initialisation", "TFT + Touch + UART");
  showCalibrationHintAndMaybeRun();

  g_ui.begin();
  Serial1.setRX(ui_config::kPinUartRx);
  Serial1.setTX(ui_config::kPinUartTx);
  g_uart.begin(Serial1, ui_config::kSerialBaud, ui_config::kPinUartRx, ui_config::kPinUartTx);
  g_uart.setJsonHandler(handleIncomingJson, nullptr);
  g_uart.sendRequestState();

  g_nextTouchPollMs = millis();
  g_nextRenderMs = millis();
  g_forceRender = true;
}

void loop() {
  const uint32_t nowMs = millis();
  g_uart.poll();
  g_ui.updateConnection(nowMs);
  if (g_ui.shouldRequestState(nowMs)) {
    g_uart.sendRequestState();
  }

  processTouch(nowMs);

  const bool dirty = g_ui.consumeDirty();
  if (g_forceRender || dirty || static_cast<int32_t>(nowMs - g_nextRenderMs) >= 0) {
    const bool forceFull = g_forceRender;
    g_renderer.render(g_ui, nowMs, forceFull);
    g_forceRender = false;
    g_nextRenderMs =
        nowMs + ((dirty || forceFull) ? ui_config::kRenderDirtyFramePeriodMs : ui_config::kRenderIdleFramePeriodMs);
  }
}
