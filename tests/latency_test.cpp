#include "../internal/clock.h"
#include "../internal/platform.h"

#include <cassert>
#include <cstdint>
#include <cstdio>

int main() {
    // Read TSC twice and ensure monotonicity (or equality on fallback).
    const std::uint64_t t0 = logger::internal::ReadTsc();
    const std::uint64_t t1 = logger::internal::ReadTsc();
    assert(t1 >= t0);

    // Convert to nanoseconds using the calibrated conversion.
    const std::uint64_t n0 = logger::internal::TscToNanoseconds(t0);
    const std::uint64_t n1 = logger::internal::TscToNanoseconds(t1);
    assert(n1 >= n0);

    // Simple loop to get a rough per-call overhead estimate (informational).
    constexpr std::uint64_t kIterations = 100000;
    const std::uint64_t start = logger::internal::ReadTsc();
    for (std::uint64_t i = 0; i < kIterations; ++i) {
        (void)logger::internal::ReadTsc();
    }
    const std::uint64_t end = logger::internal::ReadTsc();
    const double avg_ticks = (kIterations > 0) ? static_cast<double>(end - start) / kIterations : 0.0;

    std::printf("avg_tsc_ticks_per_call=%.2f\n", avg_ticks);
    return 0;
}
