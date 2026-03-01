#include "system/boot_report.h"

#if defined(ARDUINO_ARCH_ESP32)
#include <esp_heap_caps.h>
#include <esp_system.h>
#endif

namespace {

const char* kUnknownResetReason = "unknown";

}  // namespace

const char* bootResetReasonLabel(uint32_t reset_reason_code) {
#if defined(ARDUINO_ARCH_ESP32)
  switch (static_cast<esp_reset_reason_t>(reset_reason_code)) {
    case ESP_RST_UNKNOWN:
      return "unknown";
    case ESP_RST_POWERON:
      return "power_on";
    case ESP_RST_EXT:
      return "external";
    case ESP_RST_SW:
      return "software";
    case ESP_RST_PANIC:
      return "panic";
    case ESP_RST_INT_WDT:
      return "int_wdt";
    case ESP_RST_TASK_WDT:
      return "task_wdt";
    case ESP_RST_WDT:
      return "other_wdt";
    case ESP_RST_DEEPSLEEP:
      return "deepsleep";
    case ESP_RST_BROWNOUT:
      return "brownout";
    case ESP_RST_SDIO:
      return "sdio";
#if defined(ESP_RST_USB)
    case ESP_RST_USB:
      return "usb";
#endif
#if defined(ESP_RST_JTAG)
    case ESP_RST_JTAG:
      return "jtag";
#endif
#if defined(ESP_RST_EFUSE)
    case ESP_RST_EFUSE:
      return "efuse";
#endif
#if defined(ESP_RST_PWR_GLITCH)
    case ESP_RST_PWR_GLITCH:
      return "pwr_glitch";
#endif
#if defined(ESP_RST_CPU_LOCKUP)
    case ESP_RST_CPU_LOCKUP:
      return "cpu_lockup";
#endif
    default:
      break;
  }
#endif
  return kUnknownResetReason;
}

uint32_t bootResetReasonCode() {
#if defined(ARDUINO_ARCH_ESP32)
  return static_cast<uint32_t>(esp_reset_reason());
#else
  return 0U;
#endif
}

BootHeapSnapshot bootCaptureHeapSnapshot() {
  BootHeapSnapshot snapshot = {};
#if defined(ARDUINO_ARCH_ESP32)
  snapshot.heap_internal_free = static_cast<uint32_t>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
  snapshot.heap_internal_largest = static_cast<uint32_t>(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));
  snapshot.heap_psram_free = static_cast<uint32_t>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
  snapshot.heap_psram_largest = static_cast<uint32_t>(heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM));
  snapshot.psram_total = static_cast<uint32_t>(ESP.getPsramSize());
  snapshot.psram_found = snapshot.psram_total > 0U;
#endif
  return snapshot;
}

void bootPrintReport(const char* firmware_name, const char* firmware_version) {
  const char* safe_name = (firmware_name != nullptr && firmware_name[0] != '\0') ? firmware_name : "freenove";
  const char* safe_version = (firmware_version != nullptr && firmware_version[0] != '\0') ? firmware_version : "dev";
  const char* build_id = __DATE__ " " __TIME__;

  const uint32_t reset_reason = bootResetReasonCode();

  const BootHeapSnapshot heap = bootCaptureHeapSnapshot();

  Serial.printf("[BOOT] fw=%s version=%s build=%s\n", safe_name, safe_version, build_id);
  Serial.printf("[BOOT] reset_reason=%lu (%s)\n",
                static_cast<unsigned long>(reset_reason),
                bootResetReasonLabel(reset_reason));
  Serial.printf("[BOOT] psram_found=%u psram_total=%lu psram_free=%lu psram_largest=%lu\n",
                heap.psram_found ? 1U : 0U,
                static_cast<unsigned long>(heap.psram_total),
                static_cast<unsigned long>(heap.heap_psram_free),
                static_cast<unsigned long>(heap.heap_psram_largest));
  Serial.printf("[BOOT] heap_internal_free=%lu heap_internal_largest=%lu\n",
                static_cast<unsigned long>(heap.heap_internal_free),
                static_cast<unsigned long>(heap.heap_internal_largest));
}
