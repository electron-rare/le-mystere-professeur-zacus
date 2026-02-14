#include "serial_router.h"

#include <cctype>
#include <cstring>
#include <cstdio>

SerialRouter::SerialRouter(HardwareSerial& serial) : serial_(serial) {}

void SerialRouter::setDispatcher(DispatchFn dispatcher, void* ctx) {
  dispatcher_ = dispatcher;
  dispatcherCtx_ = ctx;
}

void SerialRouter::trim(char* line) {
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
    line[dst++] = line[i];
  }
  line[dst] = '\0';
}

void SerialRouter::extractToken(const char* line,
                                char* outToken,
                                size_t outTokenLen,
                                const char** outArgs) {
  if (outToken == nullptr || outTokenLen == 0U) {
    return;
  }
  outToken[0] = '\0';
  if (outArgs != nullptr) {
    *outArgs = "";
  }
  if (line == nullptr || line[0] == '\0') {
    return;
  }

  size_t src = 0U;
  size_t dst = 0U;
  while (line[src] != '\0' && isspace(static_cast<unsigned char>(line[src])) == 0) {
    if (dst < (outTokenLen - 1U)) {
      outToken[dst++] = static_cast<char>(toupper(static_cast<unsigned char>(line[src])));
    }
    ++src;
  }
  outToken[dst] = '\0';

  while (line[src] != '\0' && isspace(static_cast<unsigned char>(line[src])) != 0) {
    ++src;
  }
  if (outArgs != nullptr) {
    *outArgs = &line[src];
  }
}

void SerialRouter::update(uint32_t nowMs) {
  while (serial_.available() > 0) {
    const char c = static_cast<char>(serial_.read());
    if (c == '\r') {
      continue;
    }

    if (c == '\n') {
      buffer_[len_] = '\0';
      trim(buffer_);
      if (buffer_[0] != '\0' && dispatcher_ != nullptr) {
        const char* args = "";
        extractToken(buffer_, token_, sizeof(token_), &args);
        SerialCommand cmd;
        cmd.line = buffer_;
        cmd.token = token_;
        cmd.args = args;
        dispatcher_(cmd, nowMs, dispatcherCtx_);
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
