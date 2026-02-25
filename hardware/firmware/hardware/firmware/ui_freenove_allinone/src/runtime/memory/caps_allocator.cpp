#include "runtime/memory/caps_allocator.h"

#include <Arduino.h>

#if defined(ARDUINO_ARCH_ESP32)
#include <esp_heap_caps.h>
#endif

#include <cstdlib>

namespace runtime {
namespace memory {

namespace {

uint32_t g_alloc_failures = 0U;

void noteAllocFailure(size_t bytes, const char* tag, const char* source) {
  ++g_alloc_failures;
  Serial.printf("[MEM] alloc_fail source=%s bytes=%u tag=%s fail_count=%lu\n",
                source,
                static_cast<unsigned int>(bytes),
                (tag != nullptr) ? tag : "n/a",
                static_cast<unsigned long>(g_alloc_failures));
}

}  // namespace

void* CapsAllocator::allocInternalDmaAligned(size_t alignment,
                                             size_t bytes,
                                             const char* tag,
                                             bool* out_used_fallback) {
  if (out_used_fallback != nullptr) {
    *out_used_fallback = false;
  }
  if (bytes == 0U) {
    return nullptr;
  }
  if (alignment < sizeof(void*)) {
    alignment = sizeof(void*);
  }

#if defined(ARDUINO_ARCH_ESP32)
  void* ptr = heap_caps_aligned_alloc(alignment,
                                      bytes,
                                      MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  if (ptr != nullptr) {
    return ptr;
  }
  ptr = heap_caps_aligned_alloc(alignment, bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  if (ptr != nullptr) {
    if (out_used_fallback != nullptr) {
      *out_used_fallback = true;
    }
    Serial.printf("[MEM] alloc_fallback source=INTERNAL_DMA_ALIGNED->INTERNAL_ALIGNED bytes=%u tag=%s align=%u\n",
                  static_cast<unsigned int>(bytes),
                  (tag != nullptr) ? tag : "n/a",
                  static_cast<unsigned int>(alignment));
    return ptr;
  }
  noteAllocFailure(bytes, tag, "INTERNAL_DMA_ALIGNED");
  return nullptr;
#else
  void* ptr = nullptr;
  const int rc = posix_memalign(&ptr, alignment, bytes);
  if (rc != 0 || ptr == nullptr) {
    noteAllocFailure(bytes, tag, "ALIGNED_MALLOC");
    return nullptr;
  }
  return ptr;
#endif
}

void* CapsAllocator::allocInternalDma(size_t bytes, const char* tag, bool* out_used_fallback) {
  if (out_used_fallback != nullptr) {
    *out_used_fallback = false;
  }
  if (bytes == 0U) {
    return nullptr;
  }
#if defined(ARDUINO_ARCH_ESP32)
  void* ptr = heap_caps_malloc(bytes, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  if (ptr != nullptr) {
    return ptr;
  }
  ptr = heap_caps_malloc(bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  if (ptr != nullptr) {
    if (out_used_fallback != nullptr) {
      *out_used_fallback = true;
    }
    Serial.printf("[MEM] alloc_fallback source=INTERNAL_DMA->INTERNAL bytes=%u tag=%s\n",
                  static_cast<unsigned int>(bytes),
                  (tag != nullptr) ? tag : "n/a");
    return ptr;
  }
  noteAllocFailure(bytes, tag, "INTERNAL_DMA");
  return nullptr;
#else
  (void)tag;
  void* ptr = std::malloc(bytes);
  if (ptr == nullptr) {
    noteAllocFailure(bytes, tag, "MALLOC");
  }
  return ptr;
#endif
}

void* CapsAllocator::allocPsram(size_t bytes, const char* tag, bool* out_used_fallback) {
  if (out_used_fallback != nullptr) {
    *out_used_fallback = false;
  }
  if (bytes == 0U) {
    return nullptr;
  }
#if defined(ARDUINO_ARCH_ESP32)
  void* ptr = heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (ptr != nullptr) {
    return ptr;
  }
  ptr = heap_caps_malloc(bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  if (ptr != nullptr) {
    if (out_used_fallback != nullptr) {
      *out_used_fallback = true;
    }
    Serial.printf("[MEM] alloc_fallback source=PSRAM->INTERNAL bytes=%u tag=%s\n",
                  static_cast<unsigned int>(bytes),
                  (tag != nullptr) ? tag : "n/a");
    return ptr;
  }
  noteAllocFailure(bytes, tag, "PSRAM");
  return nullptr;
#else
  (void)tag;
  void* ptr = std::malloc(bytes);
  if (ptr == nullptr) {
    noteAllocFailure(bytes, tag, "MALLOC");
  }
  return ptr;
#endif
}

void* CapsAllocator::allocDefault(size_t bytes, const char* tag) {
  if (bytes == 0U) {
    return nullptr;
  }
#if defined(ARDUINO_ARCH_ESP32)
  void* ptr = heap_caps_malloc(bytes, MALLOC_CAP_8BIT);
  if (ptr == nullptr) {
    noteAllocFailure(bytes, tag, "DEFAULT");
  }
  return ptr;
#else
  (void)tag;
  void* ptr = std::malloc(bytes);
  if (ptr == nullptr) {
    noteAllocFailure(bytes, tag, "MALLOC");
  }
  return ptr;
#endif
}

void CapsAllocator::release(void* ptr) {
  if (ptr == nullptr) {
    return;
  }
  std::free(ptr);
}

uint32_t CapsAllocator::failureCount() {
  return g_alloc_failures;
}

}  // namespace memory
}  // namespace runtime
