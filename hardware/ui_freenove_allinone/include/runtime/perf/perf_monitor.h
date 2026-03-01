// perf_monitor.h - lightweight runtime performance counters.
#pragma once

#include <Arduino.h>

#include <cstdint>

enum class PerfSection : uint8_t {
  kLoop = 0,
  kUiTick,
  kUiFlush,
  kScenarioTick,
  kNetworkUpdate,
  kAudioUpdate,
  kCount,
};

struct PerfSectionStats {
  uint32_t count = 0U;
  uint64_t total_us = 0ULL;
  uint32_t max_us = 0U;
};

struct PerfSnapshot {
  PerfSectionStats loop = {};
  PerfSectionStats ui_tick = {};
  PerfSectionStats ui_flush = {};
  PerfSectionStats scenario_tick = {};
  PerfSectionStats network_update = {};
  PerfSectionStats audio_update = {};
  uint32_t ui_dma_flush_count = 0U;
  uint32_t ui_sync_flush_count = 0U;
};

class PerfMonitor {
 public:
  void reset();
  uint32_t beginSample() const;
  void endSample(PerfSection section, uint32_t started_us);
  void noteUiFlush(bool dma_used, uint32_t elapsed_us);
  PerfSnapshot snapshot() const;
  void dumpStatus() const;

 private:
  void noteSection(PerfSection section, uint32_t elapsed_us);
  static uint32_t elapsedUs(uint32_t started_us, uint32_t ended_us);

  PerfSectionStats sections_[static_cast<uint8_t>(PerfSection::kCount)] = {};
  uint32_t ui_dma_flush_count_ = 0U;
  uint32_t ui_sync_flush_count_ = 0U;
};

PerfMonitor& perfMonitor();
