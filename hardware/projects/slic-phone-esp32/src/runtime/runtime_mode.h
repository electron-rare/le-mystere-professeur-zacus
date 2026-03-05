#pragma once

#include <Arduino.h>

enum class RuntimeMode : uint8_t {
  kSignal = 0,
  kMp3 = 1,
};
