// caps_allocator.h - capability-aware allocator wrappers.
#pragma once

#include <cstddef>
#include <cstdint>

namespace runtime {
namespace memory {

class CapsAllocator {
 public:
  static void* allocInternalDma(size_t bytes, const char* tag, bool* out_used_fallback = nullptr);
  static void* allocPsram(size_t bytes, const char* tag, bool* out_used_fallback = nullptr);
  static void* allocDefault(size_t bytes, const char* tag);
  static void release(void* ptr);

  static uint32_t failureCount();
};

}  // namespace memory
}  // namespace runtime
