#include "runtime/perf/perf_monitor.h"

namespace {

PerfMonitor g_perf_monitor;

const PerfSectionStats* sectionStatsFor(const PerfSnapshot& snapshot, PerfSection section) {
  switch (section) {
    case PerfSection::kLoop:
      return &snapshot.loop;
    case PerfSection::kUiTick:
      return &snapshot.ui_tick;
    case PerfSection::kUiFlush:
      return &snapshot.ui_flush;
    case PerfSection::kScenarioTick:
      return &snapshot.scenario_tick;
    case PerfSection::kNetworkUpdate:
      return &snapshot.network_update;
    case PerfSection::kAudioUpdate:
      return &snapshot.audio_update;
    case PerfSection::kCount:
      break;
  }
  return nullptr;
}

const char* sectionLabel(PerfSection section) {
  switch (section) {
    case PerfSection::kLoop:
      return "loop";
    case PerfSection::kUiTick:
      return "ui_tick";
    case PerfSection::kUiFlush:
      return "ui_flush";
    case PerfSection::kScenarioTick:
      return "scenario_tick";
    case PerfSection::kNetworkUpdate:
      return "network_update";
    case PerfSection::kAudioUpdate:
      return "audio_update";
    case PerfSection::kCount:
      break;
  }
  return "unknown";
}

}  // namespace

void PerfMonitor::reset() {
  for (uint8_t index = 0U; index < static_cast<uint8_t>(PerfSection::kCount); ++index) {
    sections_[index] = {};
  }
  ui_dma_flush_count_ = 0U;
  ui_sync_flush_count_ = 0U;
}

uint32_t PerfMonitor::beginSample() const {
  return micros();
}

void PerfMonitor::endSample(PerfSection section, uint32_t started_us) {
  const uint32_t ended_us = micros();
  noteSection(section, elapsedUs(started_us, ended_us));
}

void PerfMonitor::noteUiFlush(bool dma_used, uint32_t elapsed_us) {
  noteSection(PerfSection::kUiFlush, elapsed_us);
  if (dma_used) {
    ++ui_dma_flush_count_;
  } else {
    ++ui_sync_flush_count_;
  }
}

PerfSnapshot PerfMonitor::snapshot() const {
  PerfSnapshot out = {};
  out.loop = sections_[static_cast<uint8_t>(PerfSection::kLoop)];
  out.ui_tick = sections_[static_cast<uint8_t>(PerfSection::kUiTick)];
  out.ui_flush = sections_[static_cast<uint8_t>(PerfSection::kUiFlush)];
  out.scenario_tick = sections_[static_cast<uint8_t>(PerfSection::kScenarioTick)];
  out.network_update = sections_[static_cast<uint8_t>(PerfSection::kNetworkUpdate)];
  out.audio_update = sections_[static_cast<uint8_t>(PerfSection::kAudioUpdate)];
  out.ui_dma_flush_count = ui_dma_flush_count_;
  out.ui_sync_flush_count = ui_sync_flush_count_;
  return out;
}

void PerfMonitor::dumpStatus() const {
  const PerfSnapshot snap = snapshot();
  for (uint8_t index = 0U; index < static_cast<uint8_t>(PerfSection::kCount); ++index) {
    const PerfSection section = static_cast<PerfSection>(index);
    const PerfSectionStats* stats = sectionStatsFor(snap, section);
    if (stats == nullptr) {
      continue;
    }
    const uint32_t avg_us = (stats->count == 0U)
                                ? 0U
                                : static_cast<uint32_t>(stats->total_us / stats->count);
    Serial.printf("[PERF] %s count=%lu avg_us=%lu max_us=%lu\n",
                  sectionLabel(section),
                  static_cast<unsigned long>(stats->count),
                  static_cast<unsigned long>(avg_us),
                  static_cast<unsigned long>(stats->max_us));
  }
  Serial.printf("[PERF] ui_flush_dma=%lu ui_flush_sync=%lu\n",
                static_cast<unsigned long>(snap.ui_dma_flush_count),
                static_cast<unsigned long>(snap.ui_sync_flush_count));
}

void PerfMonitor::noteSection(PerfSection section, uint32_t elapsed_us) {
  const uint8_t index = static_cast<uint8_t>(section);
  if (index >= static_cast<uint8_t>(PerfSection::kCount)) {
    return;
  }
  PerfSectionStats& stats = sections_[index];
  ++stats.count;
  stats.total_us += static_cast<uint64_t>(elapsed_us);
  if (elapsed_us > stats.max_us) {
    stats.max_us = elapsed_us;
  }
}

uint32_t PerfMonitor::elapsedUs(uint32_t started_us, uint32_t ended_us) {
  return (ended_us >= started_us)
             ? (ended_us - started_us)
             : (0xFFFFFFFFUL - started_us + ended_us + 1UL);
}

PerfMonitor& perfMonitor() {
  return g_perf_monitor;
}
