#pragma once

#include <Arduino.h>

namespace ui_config {

constexpr uint16_t kScreenWidth = 480;
constexpr uint16_t kScreenHeight = 320;

constexpr uint8_t kPinSpiSck = 2;
constexpr uint8_t kPinSpiMosi = 3;
constexpr uint8_t kPinSpiMiso = 4;

constexpr uint8_t kPinTftCs = 5;
constexpr uint8_t kPinTftDc = 6;
constexpr uint8_t kPinTftRst = 7;

constexpr uint8_t kPinTouchCs = 9;
#ifdef UI_TOUCH_IRQ_PIN
constexpr int8_t kPinTouchIrq = static_cast<int8_t>(UI_TOUCH_IRQ_PIN);
#else
constexpr int8_t kPinTouchIrq = 15;
#endif

#ifdef UI_UART_TX_PIN
constexpr int8_t kPinUartTx = static_cast<int8_t>(UI_UART_TX_PIN);
#else
constexpr int8_t kPinUartTx = 0;
#endif

#ifdef UI_UART_RX_PIN
constexpr int8_t kPinUartRx = static_cast<int8_t>(UI_UART_RX_PIN);
#else
constexpr int8_t kPinUartRx = 1;
#endif

#ifdef UI_SERIAL_BAUD
constexpr uint32_t kSerialBaud = static_cast<uint32_t>(UI_SERIAL_BAUD);
#else
constexpr uint32_t kSerialBaud = 57600U;  // Match ESP32 UI Link v2 default
#endif

#ifdef UI_ROTATION
constexpr uint8_t kRotation = static_cast<uint8_t>(UI_ROTATION);
#else
constexpr uint8_t kRotation = 1U;
#endif

constexpr uint32_t kTouchPollPeriodMs = 20U;      // 50 Hz
constexpr uint32_t kTouchDebounceMs = 70U;
constexpr uint16_t kTapMaxTravelPx = 18U;
constexpr uint16_t kSwipeMinTravelPx = 52U;
constexpr uint32_t kGestureMaxTapMs = 450U;

constexpr uint32_t kRenderDirtyFramePeriodMs = 34U;   // ~30 FPS max
constexpr uint32_t kRenderIdleFramePeriodMs = 200U;   // 5 FPS idle

constexpr uint32_t kHbTimeoutMs = 3000U;
constexpr uint32_t kRequestStateRetryMs = 1000U;

constexpr uint32_t kTxtMarqueeStepMs = 120U;
constexpr uint16_t kTxtMarqueeStartDelayMs = 900U;

constexpr char kCalibrationPath[] = "/touch_cal_v1.json";

}  // namespace ui_config
