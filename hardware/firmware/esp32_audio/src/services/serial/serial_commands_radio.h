#pragma once

#include <Arduino.h>

#include "serial_router.h"

class WifiService;
class RadioService;
class WebUiService;

struct RadioSerialRuntimeContext {
  WifiService* wifi = nullptr;
  RadioService* radio = nullptr;
  WebUiService* web = nullptr;
  void (*printHelp)() = nullptr;
};

bool serialIsRadioCommand(const char* token);
bool serialProcessRadioCommand(const SerialCommand& cmd,
                               uint32_t nowMs,
                               const RadioSerialRuntimeContext& ctx,
                               Print& out);
