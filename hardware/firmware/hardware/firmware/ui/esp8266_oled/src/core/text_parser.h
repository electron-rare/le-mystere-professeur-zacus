#pragma once

#include <Arduino.h>

#include "text_slots.h"

namespace screen_core {

bool parseTxtFrame(const char* frame,
                   TextSlots* slots,
                   uint32_t* crcErrorCount,
                   uint32_t* parseErrorCount);

}  // namespace screen_core
