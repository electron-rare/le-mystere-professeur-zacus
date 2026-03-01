#include "text_slots.h"

#include <cstring>

namespace screen_core {

namespace {

constexpr const char* kTokens[] = {
    "NP_TITLE1",
    "NP_TITLE2",
    "NP_SUB",
    "LIST_PATH",
    "LIST_ROW0",
    "LIST_ROW1",
    "LIST_ROW2",
    "SET_HINT",
};

}  // namespace

const char* textSlotToken(TextSlotId id) {
  const uint8_t idx = static_cast<uint8_t>(id);
  if (idx >= static_cast<uint8_t>(TextSlotId::kCount)) {
    return "UNK";
  }
  return kTokens[idx];
}

bool textSlotFromToken(const char* token, TextSlotId* outId) {
  if (token == nullptr || outId == nullptr) {
    return false;
  }
  for (uint8_t i = 0U; i < static_cast<uint8_t>(TextSlotId::kCount); ++i) {
    if (strcmp(token, kTokens[i]) == 0) {
      *outId = static_cast<TextSlotId>(i);
      return true;
    }
  }
  return false;
}

void clearTextSlots(TextSlots* slots) {
  if (slots == nullptr) {
    return;
  }
  slots->seq = 0U;
  for (uint8_t i = 0U; i < static_cast<uint8_t>(TextSlotId::kCount); ++i) {
    slots->slot[i][0] = '\0';
  }
}

const char* textSlotValue(const TextSlots& slots, TextSlotId id) {
  const uint8_t idx = static_cast<uint8_t>(id);
  if (idx >= static_cast<uint8_t>(TextSlotId::kCount)) {
    return "";
  }
  return slots.slot[idx];
}

void setTextSlot(TextSlots* slots, TextSlotId id, const char* text, uint32_t seq) {
  if (slots == nullptr) {
    return;
  }
  const uint8_t idx = static_cast<uint8_t>(id);
  if (idx >= static_cast<uint8_t>(TextSlotId::kCount)) {
    return;
  }
  snprintf(slots->slot[idx], sizeof(slots->slot[idx]), "%s", (text != nullptr) ? text : "");
  slots->seq = seq;
}

}  // namespace screen_core
