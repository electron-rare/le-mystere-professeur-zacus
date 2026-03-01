// safe_size.h - overflow-safe size helpers.
#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>

namespace runtime {
namespace memory {

inline bool safeMulSize(size_t lhs, size_t rhs, size_t* out_value) {
  if (out_value == nullptr) {
    return false;
  }
  if (lhs == 0U || rhs == 0U) {
    *out_value = 0U;
    return true;
  }
  if (lhs > (std::numeric_limits<size_t>::max() / rhs)) {
    return false;
  }
  *out_value = lhs * rhs;
  return true;
}

}  // namespace memory
}  // namespace runtime
