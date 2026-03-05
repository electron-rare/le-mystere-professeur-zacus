#pragma once

#include <Arduino.h>

enum class ScreenTextSlot : uint8_t {
  kNowTitle1 = 0,
  kNowTitle2 = 1,
  kNowSub = 2,
  kListPath = 3,
  kListRow0 = 4,
  kListRow1 = 5,
  kListRow2 = 6,
  kSettingHint = 7,
  kCount = 8,
};

const char* screenTextSlotToken(ScreenTextSlot slot);
bool screenTextSlotFromToken(const char* token, ScreenTextSlot* outSlot);
void sanitizeScreenText(char* text, size_t len);
