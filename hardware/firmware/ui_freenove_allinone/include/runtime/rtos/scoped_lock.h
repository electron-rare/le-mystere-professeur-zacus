// scoped_lock.h - RAII wrapper for FreeRTOS mutex
#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

namespace runtime {
namespace rtos {

/**
 * RAII wrapper for FreeRTOS mutex (SemaphoreHandle_t).
 * Auto-releases on scope exit, exception-safe.
 */
class ScopedMutexLock {
 public:
  explicit ScopedMutexLock(SemaphoreHandle_t mutex, TickType_t timeout = portMAX_DELAY)
      : mutex_(mutex), locked_(false) {
    if (mutex_ != nullptr) {
      locked_ = (xSemaphoreTake(mutex_, timeout) == pdTRUE);
    }
  }

  ~ScopedMutexLock() {
    if (locked_ && mutex_ != nullptr) {
      xSemaphoreGive(mutex_);
    }
  }

  // Non-copyable, non-movable
  ScopedMutexLock(const ScopedMutexLock&) = delete;
  ScopedMutexLock& operator=(const ScopedMutexLock&) = delete;
  ScopedMutexLock(ScopedMutexLock&&) = delete;
  ScopedMutexLock& operator=(ScopedMutexLock&&) = delete;

  /** Returns true if lock acquisition succeeded */
  operator bool() const { return locked_; }

  /** Explicit check for lock success */
  bool isLocked() const { return locked_; }

 private:
  SemaphoreHandle_t mutex_;
  bool locked_;
};

/**
 * RAII wrapper for FreeRTOS mutex creation/deletion.
 * Auto-deletes mutex on scope exit.
 */
class AutoMutex {
 public:
  AutoMutex() : handle_(xSemaphoreCreateMutex()) {}

  ~AutoMutex() {
    if (handle_ != nullptr) {
      vSemaphoreDelete(handle_);
    }
  }

  // Non-copyable, non-movable
  AutoMutex(const AutoMutex&) = delete;
  AutoMutex& operator=(const AutoMutex&) = delete;
  AutoMutex(AutoMutex&&) = delete;
  AutoMutex& operator=(AutoMutex&&) = delete;

  SemaphoreHandle_t get() const { return handle_; }
  operator bool() const { return handle_ != nullptr; }

 private:
  SemaphoreHandle_t handle_;
};

}  // namespace rtos
}  // namespace runtime
