#pragma once

#include <Arduino.h>

#include "ui_serial_types.h"

using UiSerialCommandHandler = bool (*)(const UiSerialCommand& command, void* ctx);

void uiSerialInit(HardwareSerial& serial, uint32_t baud, int8_t rxPin, int8_t txPin);
void uiSerialSetCommandHandler(UiSerialCommandHandler handler, void* ctx);
void uiSerialPoll(uint32_t nowMs);
void uiSerialPublishState(const UiSerialState& state);
void uiSerialPublishTick(const UiSerialTick& tick);
void uiSerialPublishHeartbeat(uint32_t nowMs);
void uiSerialPublishList(const UiSerialList& list);
bool uiSerialIsReady();
