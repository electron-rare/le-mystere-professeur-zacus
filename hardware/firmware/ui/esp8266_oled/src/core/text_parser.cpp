#include "text_parser.h"

#include <cstdio>
#include <cstring>

namespace screen_core {

namespace {

uint8_t crc8(const uint8_t* data, size_t len) {
  uint8_t crc = 0x00U;
  for (size_t i = 0; i < len; ++i) {
    crc ^= data[i];
    for (uint8_t bit = 0U; bit < 8U; ++bit) {
      if ((crc & 0x80U) != 0U) {
        crc = static_cast<uint8_t>((crc << 1U) ^ 0x07U);
      } else {
        crc <<= 1U;
      }
    }
  }
  return crc;
}

}  // namespace

bool parseTxtFrame(const char* frame,
                   TextSlots* slots,
                   uint32_t* crcErrorCount,
                   uint32_t* parseErrorCount) {
  if (frame == nullptr || slots == nullptr || strncmp(frame, "TXT,", 4) != 0) {
    return false;
  }

  unsigned long seq = 0UL;
  char slotToken[20] = {};
  char text[64] = {};
  unsigned int crcHex = 0U;
  const int parsed =
      sscanf(frame, "TXT,%lu,%19[^,],%63[^,],%x", &seq, slotToken, text, &crcHex);
  if (parsed < 3) {
    if (parseErrorCount != nullptr) {
      ++(*parseErrorCount);
    }
    return false;
  }

  if (parsed >= 4) {
    const char* lastComma = strrchr(frame, ',');
    if (lastComma == nullptr) {
      if (parseErrorCount != nullptr) {
        ++(*parseErrorCount);
      }
      return false;
    }
    const size_t payloadLen = static_cast<size_t>(lastComma - frame);
    const uint8_t computed = crc8(reinterpret_cast<const uint8_t*>(frame), payloadLen);
    const uint8_t expected = static_cast<uint8_t>(crcHex & 0xFFU);
    if (computed != expected) {
      if (crcErrorCount != nullptr) {
        ++(*crcErrorCount);
      }
      return false;
    }
  }

  TextSlotId slot = TextSlotId::kNowTitle1;
  if (!textSlotFromToken(slotToken, &slot)) {
    if (parseErrorCount != nullptr) {
      ++(*parseErrorCount);
    }
    return false;
  }
  setTextSlot(slots, slot, text, static_cast<uint32_t>(seq));
  return true;
}

}  // namespace screen_core
