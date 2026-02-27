#pragma once

#include <Arduino.h>

namespace usb_host_runtime {

inline bool enableHostPortPower() {
#if defined(ARDUINO_ESP32_S3_USB_OTG) && defined(USB_HOST_EN) && \
    defined(USB_HOST_POWER_VBUS) && defined(USB_HOST_POWER_OFF)
    usbHostEnable(true);
    usbHostPower(USB_HOST_POWER_VBUS);
    return true;
#else
    return false;
#endif
}

}  // namespace usb_host_runtime
