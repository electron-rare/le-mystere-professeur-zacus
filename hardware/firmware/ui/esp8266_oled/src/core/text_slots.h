#pragma once

#include <Arduino.h>

namespace screen_core {

enum class TextSlotId : uint8_t {
  kNowTitle1 = 0,
  kNowTitle2 = 1,
  kNowSub = 2,
  kListPath = 3,
  kListRow0 = 4,
  kListRow1 = 5,
  kListRow2 = 6,
  kSetHint = 7,
  kCount = 8,
};

struct TextSlots {
  uint32_t seq = 0U;
  char slot[static_cast<uint8_t>(TextSlotId::kCount)][48] = {};
};

const char* textSlotToken(TextSlotId id);
bool textSlotFromToken(const char* token, TextSlotId* outId);
void clearTextSlots(TextSlots* slots);
const char* textSlotValue(const TextSlots& slots, TextSlotId id);
void setTextSlot(TextSlots* slots, TextSlotId id, const char* text, uint32_t seq);

}  // namespace screen_core
