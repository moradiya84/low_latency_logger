#include "../internal/clock.h"
#include "../internal/platform.h"

#include <chrono>

namespace logger {
namespace internal {

std::uint64_t TscToNanoseconds(std::uint64_t tsc) noexcept {
    struct Calibration {
        double ticks_per_ns;
    };

    static const Calibration calibration = []() {
        using clock = std::chrono::steady_clock;
        auto t0 = clock::now();
        std::uint64_t c0 = ReadTsc();
        auto t1 = t0;

        // Spin for a short duration to reduce quantization error.
        for (;;) {
            t1 = clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();
            if (elapsed >= 1000000) { // 1 ms
                break;
            }
        }

        std::uint64_t c1 = ReadTsc();
        auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();
        double ticks_per_ns = (ns > 0) ? static_cast<double>(c1 - c0) / static_cast<double>(ns) : 0.0;
        if (ticks_per_ns <= 0.0) {
            ticks_per_ns = 1.0;
        }
        return Calibration{ticks_per_ns};
    }();

    if (calibration.ticks_per_ns <= 0.0) {
        return 0;
    }
    return static_cast<std::uint64_t>(static_cast<double>(tsc) / calibration.ticks_per_ns);
}

} // namespace internal
} // namespace logger
