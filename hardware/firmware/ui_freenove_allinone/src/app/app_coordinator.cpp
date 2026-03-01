#include "app/app_coordinator.h"

#include "runtime/perf/perf_monitor.h"

bool AppCoordinator::begin(RuntimeServices* services) {
  if (services == nullptr) {
    return false;
  }
  services_ = services;
  return true;
}

void AppCoordinator::tick(uint32_t now_ms) {
  if (services_ == nullptr || services_->tick_runtime == nullptr) {
    return;
  }
  const uint32_t started_us = perfMonitor().beginSample();
  services_->tick_runtime(now_ms, services_);
  perfMonitor().endSample(PerfSection::kLoop, started_us);
}

void AppCoordinator::onSerialLine(const char* command_line, uint32_t now_ms) {
  if (services_ == nullptr) {
    return;
  }
  serial_router_.dispatch(command_line, now_ms, services_);
}
