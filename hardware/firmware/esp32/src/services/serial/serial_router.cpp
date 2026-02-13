#include "serial_router.h"

#include <cctype>
#include <cstring>

SerialRouter::SerialRouter(HardwareSerial& serial) : serial_(serial) {}

void SerialRouter::setDispatcher(DispatchFn dispatcher, void* ctx) {
  dispatcher_ = dispatcher;
  dispatcherCtx_ = ctx;
}

void SerialRouter::normalize(char* line) {
  if (line == nullptr) {
    return;
  }

  size_t start = 0U;
  while (line[start] != '\0' && isspace(static_cast<unsigned char>(line[start])) != 0) {
    ++start;
  }

  size_t end = strlen(line);
  while (end > start && isspace(static_cast<unsigned char>(line[end - 1U])) != 0) {
    --end;
  }

  size_t dst = 0U;
  for (size_t i = start; i < end; ++i) {
    line[dst++] = static_cast<char>(toupper(static_cast<unsigned char>(line[i])));
  }
  line[dst] = '\0';
}

void SerialRouter::update(uint32_t nowMs) {
  while (serial_.available() > 0) {
    const char c = static_cast<char>(serial_.read());
    if (c == '\r') {
      continue;
    }

    if (c == '\n') {
      buffer_[len_] = '\0';
      normalize(buffer_);
      if (buffer_[0] != '\0' && dispatcher_ != nullptr) {
        dispatcher_(buffer_, nowMs, dispatcherCtx_);
      }
      len_ = 0U;
      continue;
    }

    if (len_ < (sizeof(buffer_) - 1U)) {
      buffer_[len_++] = c;
    } else {
      len_ = 0U;
    }
  }
}
