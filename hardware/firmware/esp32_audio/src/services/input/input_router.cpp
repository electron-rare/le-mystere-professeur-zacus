#include "input_router.h"

namespace {

InputButtonAction mapButtonAction(UiBtnAction action) {
  switch (action) {
    case UI_BTN_ACTION_DOWN:
      return InputButtonAction::kDown;
    case UI_BTN_ACTION_UP:
      return InputButtonAction::kUp;
    case UI_BTN_ACTION_CLICK:
      return InputButtonAction::kClick;
    case UI_BTN_ACTION_LONG:
      return InputButtonAction::kLong;
    case UI_BTN_ACTION_UNKNOWN:
    default:
      return InputButtonAction::kUnknown;
  }
}

uint16_t mapButtonIdToLogicalKey(UiBtnId id) {
  switch (id) {
    case UI_BTN_OK:
      return 1u;
    case UI_BTN_PREV:
      return 2u;
    case UI_BTN_NEXT:
      return 3u;
    case UI_BTN_VOL_DOWN:
      return 4u;
    case UI_BTN_VOL_UP:
      return 5u;
    case UI_BTN_BACK:
    case UI_BTN_MODE:
      return 6u;
    case UI_BTN_UNKNOWN:
    default:
      return 0u;
  }
}

}  // namespace

bool InputRouter::mapUiButton(UiBtnId id,
                              UiBtnAction action,
                              uint32_t tsMs,
                              InputEvent* outEvent) const {
  if (outEvent == nullptr) {
    return false;
  }

  InputEvent event = {};
  event.source = InputEventSource::kUiSerial;
  event.type = InputEventType::kButton;
  event.code = mapButtonIdToLogicalKey(id);
  event.action = mapButtonAction(action);
  event.tsMs = tsMs;

  if (event.code == 0u || event.action == InputButtonAction::kUnknown) {
    return false;
  }

  *outEvent = event;
  return true;
}

bool InputRouter::mapUiTouch(int16_t x,
                             int16_t y,
                             UiTouchAction action,
                             uint32_t tsMs,
                             InputEvent* outEvent) const {
  if (outEvent == nullptr) {
    return false;
  }

  InputEvent event = {};
  event.source = InputEventSource::kUiSerial;
  event.type = InputEventType::kTouch;
  event.code = static_cast<uint16_t>(x < 0 ? 0 : x);
  event.value = y;
  switch (action) {
    case UI_TOUCH_ACTION_DOWN:
      event.action = InputButtonAction::kDown;
      break;
    case UI_TOUCH_ACTION_MOVE:
      event.action = InputButtonAction::kClick;
      break;
    case UI_TOUCH_ACTION_UP:
      event.action = InputButtonAction::kUp;
      break;
    case UI_TOUCH_ACTION_UNKNOWN:
    default:
      event.action = InputButtonAction::kUnknown;
      break;
  }
  event.tsMs = tsMs;

  if (event.action == InputButtonAction::kUnknown) {
    return false;
  }

  *outEvent = event;
  return true;
}
