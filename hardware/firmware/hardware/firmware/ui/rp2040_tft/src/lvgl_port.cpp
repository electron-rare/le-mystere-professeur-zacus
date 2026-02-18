#include "lvgl_port.h"

namespace {

TFT_eSPI* g_tft = nullptr;
XPT2046_Touchscreen* g_touch = nullptr;
uint16_t g_width = 0;
uint16_t g_height = 0;
uint32_t g_lastTickMs = 0;

constexpr uint16_t kTouchMin = 200;
constexpr uint16_t kTouchMax = 3900;

lv_disp_draw_buf_t g_drawBuf;
lv_color_t g_drawMem[480U * 20U];
lv_disp_drv_t g_dispDrv;
lv_indev_drv_t g_touchDrv;

int16_t mapTouchAxis(int32_t raw, int32_t inMin, int32_t inMax, int32_t outMax) {
  if (raw < inMin) {
    raw = inMin;
  }
  if (raw > inMax) {
    raw = inMax;
  }
  const int32_t spanIn = inMax - inMin;
  if (spanIn <= 0) {
    return 0;
  }
  return static_cast<int16_t>(((raw - inMin) * outMax) / spanIn);
}

void flushCallback(lv_disp_drv_t* disp, const lv_area_t* area, lv_color_t* colorP) {
  (void)disp;
  if (g_tft == nullptr) {
    lv_disp_flush_ready(disp);
    return;
  }

  const uint16_t width = static_cast<uint16_t>(area->x2 - area->x1 + 1);
  const uint16_t height = static_cast<uint16_t>(area->y2 - area->y1 + 1);

  g_tft->startWrite();
  g_tft->setAddrWindow(area->x1, area->y1, width, height);
  g_tft->pushColors(reinterpret_cast<uint16_t*>(&colorP->full), static_cast<uint32_t>(width) * height, true);
  g_tft->endWrite();

  lv_disp_flush_ready(disp);
}

void touchReadCallback(lv_indev_drv_t* drv, lv_indev_data_t* data) {
  (void)drv;
  if (g_touch == nullptr || !g_touch->touched()) {
    data->state = LV_INDEV_STATE_REL;
    return;
  }

  TS_Point p = g_touch->getPoint();
  if (p.z < 80) {
    data->state = LV_INDEV_STATE_REL;
    return;
  }

  const int16_t x = mapTouchAxis(p.x, kTouchMin, kTouchMax, g_width - 1);
  const int16_t y = mapTouchAxis(p.y, kTouchMin, kTouchMax, g_height - 1);

  data->state = LV_INDEV_STATE_PR;
  data->point.x = x;
  data->point.y = y;
}

}  // namespace

bool lvglPortInit(TFT_eSPI& tft,
                  XPT2046_Touchscreen& touch,
                  uint16_t width,
                  uint16_t height,
                  uint8_t rotation) {
  g_tft = &tft;
  g_touch = &touch;
  g_width = width;
  g_height = height;

  lv_init();

  tft.begin();
  tft.setRotation(rotation);
  tft.fillScreen(TFT_BLACK);

  lv_disp_draw_buf_init(&g_drawBuf, g_drawMem, nullptr, static_cast<uint32_t>(width) * 20U);

  lv_disp_drv_init(&g_dispDrv);
  g_dispDrv.hor_res = width;
  g_dispDrv.ver_res = height;
  g_dispDrv.flush_cb = flushCallback;
  g_dispDrv.draw_buf = &g_drawBuf;
  lv_disp_drv_register(&g_dispDrv);

  lv_indev_drv_init(&g_touchDrv);
  g_touchDrv.type = LV_INDEV_TYPE_POINTER;
  g_touchDrv.read_cb = touchReadCallback;
  lv_indev_drv_register(&g_touchDrv);

  g_lastTickMs = millis();
  return true;
}

void lvglPortTick(uint32_t nowMs) {
  if (g_lastTickMs == 0U) {
    g_lastTickMs = nowMs;
  }
  if (nowMs >= g_lastTickMs) {
    lv_tick_inc(nowMs - g_lastTickMs);
  } else {
    lv_tick_inc(1U);
  }
  g_lastTickMs = nowMs;
}
