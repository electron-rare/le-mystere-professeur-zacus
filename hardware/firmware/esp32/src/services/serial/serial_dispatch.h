#pragma once

#include <Arduino.h>

#include "serial_router.h"

enum class SerialDispatchResult : uint8_t {
  kOk = 0,
  kBadArgs,
  kOutOfContext,
  kNotFound,
  kBusy,
  kUnknown,
};

const char* serialDispatchResultLabel(SerialDispatchResult result);
void serialDispatchReply(Print& out,
                         const char* domain,
                         SerialDispatchResult result,
                         const char* detail = nullptr);

bool serialTokenEquals(const SerialCommand& cmd, const char* token);
bool serialTokenStartsWith(const SerialCommand& cmd, const char* prefix);
