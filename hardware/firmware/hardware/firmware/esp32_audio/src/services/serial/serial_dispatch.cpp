#include "serial_dispatch.h"

#include <cstring>

const char* serialDispatchResultLabel(SerialDispatchResult result) {
  switch (result) {
    case SerialDispatchResult::kOk:
      return "OK";
    case SerialDispatchResult::kBadArgs:
      return "BAD_ARGS";
    case SerialDispatchResult::kOutOfContext:
      return "OUT_OF_CONTEXT";
    case SerialDispatchResult::kNotFound:
      return "NOT_FOUND";
    case SerialDispatchResult::kBusy:
      return "BUSY";
    case SerialDispatchResult::kUnknown:
    default:
      return "UNKNOWN";
  }
}

void serialDispatchReply(Print& out,
                         const char* domain,
                         SerialDispatchResult result,
                         const char* detail) {
  out.printf("[%s] %s", (domain != nullptr && domain[0] != '\0') ? domain : "SERIAL",
             serialDispatchResultLabel(result));
  if (detail != nullptr && detail[0] != '\0') {
    out.printf(" %s", detail);
  }
  out.print('\n');
}

bool serialTokenEquals(const SerialCommand& cmd, const char* token) {
  if (cmd.token == nullptr || token == nullptr) {
    return false;
  }
  return strcmp(cmd.token, token) == 0;
}

bool serialTokenStartsWith(const SerialCommand& cmd, const char* prefix) {
  if (cmd.token == nullptr || prefix == nullptr) {
    return false;
  }
  const size_t prefixLen = strlen(prefix);
  if (prefixLen == 0U) {
    return false;
  }
  return strncmp(cmd.token, prefix, prefixLen) == 0;
}
