#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <lvgl.h>

bool lvglPortInit(TFT_eSPI& tft,
                  XPT2046_Touchscreen& touch,
                  uint16_t width,
                  uint16_t height,
                  uint8_t rotation);

void lvglPortTick(uint32_t nowMs);
