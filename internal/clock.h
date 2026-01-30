/**
 * @file clock.h
 * @brief Timestamp conversion utilities
 */

#ifndef LOGGER_INTERNAL_CLOCK_H
#define LOGGER_INTERNAL_CLOCK_H

#include <cstdint>

namespace logger {
namespace internal {

/**
 * @brief Convert TSC ticks to nanoseconds (calibrated on first use)
 *
 * Intended for the consumer thread (formatting), not the producer hot path.
 */
std::uint64_t TscToNanoseconds(std::uint64_t tsc) noexcept;

} // namespace internal
} // namespace logger

#endif // LOGGER_INTERNAL_CLOCK_H
