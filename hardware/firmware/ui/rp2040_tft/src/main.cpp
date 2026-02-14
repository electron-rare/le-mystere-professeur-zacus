#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <lvgl.h>

#include "../include/ui_config.h"
#include "lvgl_port.h"
#include "ui_link_client.h"

namespace {

TFT_eSPI g_tft;
XPT2046_Touchscreen g_touch(ui_config::kPinTouchCs, ui_config::kPinTouchIrq);
UiLinkClient g_link;

uint32_t g_lastHelloMs = 0U;
uint32_t g_lastRenderMs = 0U;
bool g_snapshotDirty = true;

struct UiSnapshot {
  char mode[12] = "SIGNAL";
  uint32_t seq = 0;
  uint32_t ms = 0;
  uint16_t track = 0;
  uint16_t trackTotal = 0;
  uint8_t volume = 0;
  int8_t tuningOffset = 0;
  uint8_t tuningConfidence = 0;
  uint8_t hold = 0;
  uint8_t key = 0;
};

UiSnapshot g_snapshot;

lv_obj_t* g_labelLink = nullptr;
lv_obj_t* g_labelMode = nullptr;
lv_obj_t* g_labelTrack = nullptr;
lv_obj_t* g_labelVolume = nullptr;
lv_obj_t* g_labelTune = nullptr;
lv_obj_t* g_labelMeta = nullptr;

bool parseUint32(const UiLinkFrame& frame, const char* key, uint32_t* outValue) {
  if (outValue == nullptr) {
    return false;
  }
  const UiLinkField* field = uiLinkFindField(&frame, key);
  if (field == nullptr || field->value[0] == '\0') {
    return false;
  }
  char* end = nullptr;
  const unsigned long parsed = strtoul(field->value, &end, 10);
  if (end == nullptr || *end != '\0') {
    return false;
  }
  *outValue = static_cast<uint32_t>(parsed);
  return true;
}

bool parseInt32(const UiLinkFrame& frame, const char* key, int32_t* outValue) {
  if (outValue == nullptr) {
    return false;
  }
  const UiLinkField* field = uiLinkFindField(&frame, key);
  if (field == nullptr || field->value[0] == '\0') {
    return false;
  }
  char* end = nullptr;
  const long parsed = strtol(field->value, &end, 10);
  if (end == nullptr || *end != '\0') {
    return false;
  }
  *outValue = static_cast<int32_t>(parsed);
  return true;
}

void renderSnapshot(uint32_t nowMs) {
  const bool linkUp = g_link.connected();
  if (g_labelLink != nullptr) {
    lv_label_set_text(g_labelLink, linkUp ? "LINK: OK" : "LINK: DOWN");
  }
  if (!linkUp) {
    if (g_labelMode != nullptr) {
      lv_label_set_text(g_labelMode, "LINK DOWN");
    }
    if (g_labelTrack != nullptr) {
      lv_label_set_text(g_labelTrack, "Waiting HELLO/ACK...");
    }
    return;
  }

  if (g_labelMode != nullptr) {
    lv_label_set_text_fmt(g_labelMode, "MODE: %s", g_snapshot.mode);
  }
  if (g_labelTrack != nullptr) {
    lv_label_set_text_fmt(g_labelTrack,
                          "TRACK: %u / %u",
                          static_cast<unsigned int>(g_snapshot.track),
                          static_cast<unsigned int>(g_snapshot.trackTotal));
  }
  if (g_labelVolume != nullptr) {
    lv_label_set_text_fmt(g_labelVolume, "VOL: %u%%  HOLD: %u%%", g_snapshot.volume, g_snapshot.hold);
  }
  if (g_labelTune != nullptr) {
    lv_label_set_text_fmt(g_labelTune,
                          "TUNE: %+d  CONF: %u%%",
                          static_cast<int>(g_snapshot.tuningOffset),
                          static_cast<unsigned int>(g_snapshot.tuningConfidence));
  }
  if (g_labelMeta != nullptr) {
    lv_label_set_text_fmt(g_labelMeta,
                          "SEQ:%lu  UP:%lus  KEY:%u",
                          static_cast<unsigned long>(g_snapshot.seq),
                          static_cast<unsigned long>(g_snapshot.ms / 1000UL),
                          static_cast<unsigned int>(g_snapshot.key));
  }
  (void)nowMs;
}

void onIncomingFrame(const UiLinkFrame& frame, uint32_t nowMs, void* ctx) {
  (void)ctx;
  if (frame.type != UILINK_MSG_STAT && frame.type != UILINK_MSG_KEYFRAME) {
    return;
  }

  const UiLinkField* mode = uiLinkFindField(&frame, "mode");
  if (mode != nullptr) {
    snprintf(g_snapshot.mode, sizeof(g_snapshot.mode), "%s", mode->value);
  }

  uint32_t u32 = 0;
  int32_t i32 = 0;
  if (parseUint32(frame, "seq", &u32)) {
    g_snapshot.seq = u32;
  }
  if (parseUint32(frame, "ms", &u32)) {
    g_snapshot.ms = u32;
  }
  if (parseUint32(frame, "track", &u32)) {
    g_snapshot.track = static_cast<uint16_t>(u32);
  }
  if (parseUint32(frame, "track_total", &u32)) {
    g_snapshot.trackTotal = static_cast<uint16_t>(u32);
  }
  if (parseUint32(frame, "vol", &u32)) {
    g_snapshot.volume = static_cast<uint8_t>(u32 > 100U ? 100U : u32);
  }
  if (parseUint32(frame, "hold", &u32)) {
    g_snapshot.hold = static_cast<uint8_t>(u32 > 100U ? 100U : u32);
  }
  if (parseUint32(frame, "tune_conf", &u32)) {
    g_snapshot.tuningConfidence = static_cast<uint8_t>(u32 > 100U ? 100U : u32);
  }
  if (parseUint32(frame, "key", &u32)) {
    g_snapshot.key = static_cast<uint8_t>(u32);
  }
  if (parseInt32(frame, "tune_off", &i32)) {
    if (i32 < -8) {
      i32 = -8;
    }
    if (i32 > 8) {
      i32 = 8;
    }
    g_snapshot.tuningOffset = static_cast<int8_t>(i32);
  }

  g_snapshotDirty = true;
  (void)nowMs;
}

void onButtonEvent(lv_event_t* event) {
  if (event == nullptr) {
    return;
  }
  if (lv_event_get_code(event) != LV_EVENT_CLICKED) {
    return;
  }
  const uintptr_t raw = reinterpret_cast<uintptr_t>(lv_event_get_user_data(event));
  const UiBtnId id = static_cast<UiBtnId>(raw);
  g_link.sendButton(id, UI_BTN_ACTION_CLICK, millis());
}

lv_obj_t* createButton(const char* label, int16_t x, UiBtnId id) {
  lv_obj_t* button = lv_btn_create(lv_scr_act());
  lv_obj_set_size(button, 72, 46);
  lv_obj_align(button, LV_ALIGN_BOTTOM_LEFT, x, -8);
  lv_obj_add_event_cb(button,
                      onButtonEvent,
                      LV_EVENT_CLICKED,
                      reinterpret_cast<void*>(static_cast<uintptr_t>(id)));
  lv_obj_t* txt = lv_label_create(button);
  lv_label_set_text(txt, label);
  lv_obj_center(txt);
  return button;
}

void buildUi() {
  lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0x101820), LV_PART_MAIN);

  g_labelLink = lv_label_create(lv_scr_act());
  lv_obj_align(g_labelLink, LV_ALIGN_TOP_LEFT, 10, 8);

  g_labelMode = lv_label_create(lv_scr_act());
  lv_obj_align(g_labelMode, LV_ALIGN_TOP_LEFT, 10, 36);

  g_labelTrack = lv_label_create(lv_scr_act());
  lv_obj_align(g_labelTrack, LV_ALIGN_TOP_LEFT, 10, 66);

  g_labelVolume = lv_label_create(lv_scr_act());
  lv_obj_align(g_labelVolume, LV_ALIGN_TOP_LEFT, 10, 96);

  g_labelTune = lv_label_create(lv_scr_act());
  lv_obj_align(g_labelTune, LV_ALIGN_TOP_LEFT, 10, 126);

  g_labelMeta = lv_label_create(lv_scr_act());
  lv_obj_align(g_labelMeta, LV_ALIGN_TOP_LEFT, 10, 156);

  createButton("PREV", 8, UI_BTN_PREV);
  createButton("NEXT", 86, UI_BTN_NEXT);
  createButton("OK", 164, UI_BTN_OK);
  createButton("BACK", 242, UI_BTN_BACK);
  createButton("VOL-", 320, UI_BTN_VOL_DOWN);
  createButton("VOL+", 398, UI_BTN_VOL_UP);

  renderSnapshot(millis());
}

void maybeSendHello(uint32_t nowMs) {
  if (g_link.connected()) {
    return;
  }
  if (g_lastHelloMs != 0U && static_cast<uint32_t>(nowMs - g_lastHelloMs) < UILINK_V2_HEARTBEAT_MS) {
    return;
  }
  if (g_link.sendHello("TFT", "rp2040-tft", "v2-lvgl", "btn:1;touch:1;ui:lvgl")) {
    g_lastHelloMs = nowMs;
  }
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(80);
  Serial.println("[UI_TFT] RP2040 TFT LVGL UI Link v2 boot");

  SPI.setSCK(ui_config::kPinSpiSck);
  SPI.setTX(ui_config::kPinSpiMosi);
  SPI.setRX(ui_config::kPinSpiMiso);
  SPI.begin();

  g_touch.begin();
  g_touch.setRotation(ui_config::kRotation);

  lvglPortInit(g_tft,
               g_touch,
               ui_config::kScreenWidth,
               ui_config::kScreenHeight,
               ui_config::kRotation);
  buildUi();

  Serial1.setRX(ui_config::kPinUartRx);
  Serial1.setTX(ui_config::kPinUartTx);
  g_link.begin(Serial1, ui_config::kSerialBaud);
  g_link.setFrameHandler(onIncomingFrame, nullptr);
  maybeSendHello(millis());
}

void loop() {
  const uint32_t nowMs = millis();

  g_link.poll(nowMs);
  maybeSendHello(nowMs);

  if (g_snapshotDirty || static_cast<uint32_t>(nowMs - g_lastRenderMs) >= 250U) {
    renderSnapshot(nowMs);
    g_snapshotDirty = false;
    g_lastRenderMs = nowMs;
  }

  lvglPortTick(nowMs);
  lv_timer_handler();
  delay(5);
}
