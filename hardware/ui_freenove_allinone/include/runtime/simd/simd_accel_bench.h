// simd_accel_bench.h - wrappers used by runtime serial commands.
#pragma once

#include "runtime/simd/simd_accel.h"

namespace runtime::simd {

bool runSelfTestCommand();
SimdBenchResult runBenchCommand(uint32_t loops, uint32_t pixels);

}  // namespace runtime::simd

