#pragma once

#include <Arduino.h>

#include <cstdint>

struct BootHeapSnapshot {
  uint32_t heap_internal_free = 0U;
  uint32_t heap_internal_largest = 0U;
  uint32_t heap_psram_free = 0U;
  uint32_t heap_psram_largest = 0U;
  uint32_t psram_total = 0U;
  bool psram_found = false;
};

const char* bootResetReasonLabel(uint32_t reset_reason_code);
uint32_t bootResetReasonCode();
BootHeapSnapshot bootCaptureHeapSnapshot();
void bootPrintReport(const char* firmware_name, const char* firmware_version);
