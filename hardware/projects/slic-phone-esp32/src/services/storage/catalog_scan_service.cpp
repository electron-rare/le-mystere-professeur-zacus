#include "catalog_scan_service.h"

void CatalogScanService::reset() {
  state_ = State::kIdle;
  forceRebuildRequested_ = false;
  queuedRequest_ = false;
  queuedForceRebuild_ = false;
  startedAtMs_ = 0U;
  finishedAtMs_ = 0U;
}

void CatalogScanService::request(bool forceRebuild) {
  if (state_ == State::kRunning) {
    queuedRequest_ = true;
    queuedForceRebuild_ = queuedForceRebuild_ || forceRebuild;
    return;
  }
  state_ = State::kRequested;
  forceRebuildRequested_ = forceRebuild;
  finishedAtMs_ = 0U;
}

void CatalogScanService::start(uint32_t nowMs) {
  if (state_ != State::kRequested) {
    return;
  }
  startedAtMs_ = nowMs;
  finishedAtMs_ = 0U;
  state_ = State::kRunning;
}

void CatalogScanService::cancel() {
  if (state_ == State::kRunning || state_ == State::kRequested) {
    state_ = State::kCanceled;
    finishedAtMs_ = millis();
    queuedRequest_ = false;
    queuedForceRebuild_ = false;
    forceRebuildRequested_ = false;
  }
}

void CatalogScanService::finish(State state, uint32_t nowMs) {
  if (state_ != State::kRunning) {
    return;
  }
  if (state != State::kDone && state != State::kFailed && state != State::kCanceled) {
    state = State::kFailed;
  }
  state_ = state;
  finishedAtMs_ = nowMs;

  if (!queuedRequest_) {
    return;
  }

  const bool forceRebuild = queuedForceRebuild_;
  queuedRequest_ = false;
  queuedForceRebuild_ = false;
  state_ = State::kRequested;
  forceRebuildRequested_ = forceRebuild;
  finishedAtMs_ = 0U;
}

bool CatalogScanService::isBusy() const {
  return state_ == State::kRunning || state_ == State::kRequested;
}

bool CatalogScanService::hasPendingRequest() const {
  return state_ == State::kRequested;
}

CatalogScanService::State CatalogScanService::state() const {
  return state_;
}

bool CatalogScanService::forceRebuildRequested() const {
  return forceRebuildRequested_;
}

uint32_t CatalogScanService::startedAtMs() const {
  return startedAtMs_;
}

uint32_t CatalogScanService::finishedAtMs() const {
  return finishedAtMs_;
}
