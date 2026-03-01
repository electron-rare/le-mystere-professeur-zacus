#include "app/runtime_serial_service.h"

void RuntimeSerialService::configure(HandleSerialCommandFn handle_serial_command,
                                     DispatchControlActionFn dispatch_control_action) {
  handle_serial_command_ = handle_serial_command;
  dispatch_control_action_ = dispatch_control_action;
}

void RuntimeSerialService::handleSerialCommand(const char* command_line, uint32_t now_ms) const {
  if (handle_serial_command_ == nullptr) {
    return;
  }
  handle_serial_command_(command_line, now_ms);
}

bool RuntimeSerialService::dispatchControlAction(const String& action_raw,
                                                 uint32_t now_ms,
                                                 String* out_error) const {
  if (dispatch_control_action_ == nullptr) {
    if (out_error != nullptr) {
      *out_error = "serial_service_unconfigured";
    }
    return false;
  }
  return dispatch_control_action_(action_raw, now_ms, out_error);
}
