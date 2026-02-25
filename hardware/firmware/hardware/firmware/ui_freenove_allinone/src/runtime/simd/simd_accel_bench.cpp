// simd_accel_bench.cpp - command wrappers for SIMD selftest and benchmarks.
#include "runtime/simd/simd_accel_bench.h"

namespace runtime::simd {

bool runSelfTestCommand() {
  return selfTest();
}

SimdBenchResult runBenchCommand(uint32_t loops, uint32_t pixels) {
  return runBench(loops, pixels);
}

}  // namespace runtime::simd

