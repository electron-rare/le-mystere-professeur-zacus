#include "screen_text_slots.h"

#include <cctype>
#include <cstring>

namespace {

constexpr const char* kSlotTokens[] = {
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

const char* screenTextSlotToken(ScreenTextSlot slot) {
  const uint8_t idx = static_cast<uint8_t>(slot);
  if (idx >= static_cast<uint8_t>(ScreenTextSlot::kCount)) {
    return "UNK";
  }
  return kSlotTokens[idx];
}

bool screenTextSlotFromToken(const char* token, ScreenTextSlot* outSlot) {
  if (token == nullptr || outSlot == nullptr) {
    return false;
  }
  for (uint8_t i = 0U; i < static_cast<uint8_t>(ScreenTextSlot::kCount); ++i) {
    if (strcmp(token, kSlotTokens[i]) == 0) {
      *outSlot = static_cast<ScreenTextSlot>(i);
      return true;
    }
  }
  return false;
}

void sanitizeScreenText(char* text, size_t len) {
  if (text == nullptr || len == 0U) {
    return;
  }
  text[len - 1U] = '\0';
  for (size_t i = 0U; i < len && text[i] != '\0'; ++i) {
    unsigned char c = static_cast<unsigned char>(text[i]);
    if (c == ',' || c == '\r' || c == '\n' || c < 0x20U) {
      text[i] = ' ';
      continue;
    }
    if (c > 0x7EU) {
      text[i] = '?';
    }
  }
  size_t start = 0U;
  while (text[start] == ' ') {
    ++start;
  }
  if (start > 0U) {
    size_t j = 0U;
    while (text[start] != '\0') {
      text[j++] = text[start++];
    }
    text[j] = '\0';
  }
  size_t n = strlen(text);
  while (n > 0U && text[n - 1U] == ' ') {
    text[n - 1U] = '\0';
    --n;
  }
}
