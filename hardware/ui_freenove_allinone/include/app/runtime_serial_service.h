// runtime_serial_service.h - dispatch bridge for serial + control actions.
#pragma once

#include <Arduino.h>

class RuntimeSerialService {
 public:
  using HandleSerialCommandFn = void (*)(const char* command_line, uint32_t now_ms);
  using DispatchControlActionFn = bool (*)(const String& action_raw, uint32_t now_ms, String* out_error);

  void configure(HandleSerialCommandFn handle_serial_command, DispatchControlActionFn dispatch_control_action);
  void handleSerialCommand(const char* command_line, uint32_t now_ms) const;
  bool dispatchControlAction(const String& action_raw, uint32_t now_ms, String* out_error) const;

 private:
  HandleSerialCommandFn handle_serial_command_ = nullptr;
  DispatchControlActionFn dispatch_control_action_ = nullptr;
};
