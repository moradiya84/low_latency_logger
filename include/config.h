#ifndef LOGGER_CONFIG_H
#define LOGGER_CONFIG_H

#include <cstddef> // for size_t

/**
 * @file config.h
 * @brief Compile-time configuration for the low-latency logger.
 *
 * This file defines macros and constants that control the behavior of the logger.
 * Users can override these defaults by defining the macros before including this header
 * or via compiler flags (e.g., -DLOGGER_MAX_MESSAGE_SIZE=2048).
 */

/**
 * @brief Maximum size of the formatted log message payload in BYTES.
 *
 * This defines the size of the static buffer used to hold the formatted log string
 * within the LogRecord. This avoids dynamic allocation during logging.
 * Larger values consume more stack/heap space per record.
 */
#ifndef LOGGER_MAX_MESSAGE_SIZE
#define LOGGER_MAX_MESSAGE_SIZE 1024
#endif

/**
 * @brief Enable thread ID in log records.
 *
 * If enabled, the thread ID is captured for every log message.
 */
#ifndef LOGGER_ENABLE_THREAD_ID
#define LOGGER_ENABLE_THREAD_ID 1
#endif

/**
 * @brief Enable source location (file, line, function) in log records.
 *
 * If enabled, __FILE__, __LINE__, and __func__ are captured.
 * this will help in debugging by providing file name, line number and function name
 */
#ifndef LOGGER_ENABLE_SOURCE_LOCATION
#define LOGGER_ENABLE_SOURCE_LOCATION 1
#endif

/**
 * @brief Enable dropping info into stdout/stderr.
 *
 * If enabled, the logger may output dropped message counts or errors to stderr.
 */
#ifndef LOGGER_ENABLE_STDERR_DIAGNOSTICS
#define LOGGER_ENABLE_STDERR_DIAGNOSTICS 1
#endif

// ============================================================================
// PERFORMANCE TUNING
// ============================================================================

/**
 * @brief Number of spin iterations in the backend consumer wait strategy
 * before falling back to a yield/sleep.
 *
 * The backend consumer will spin this many times using LOGGER_CPU_RELAX()
 * (defined in internal/platform.h) before yielding to the OS scheduler.
 *
 * so instead of letting cpu put consumer thread in sleep mode or yield which will slow down
 * it will spin for given number of times which is like keep it running
 *
 * Higher values: Lower latency, more CPU usage
 * Lower values:  Higher latency, less CPU usage
 *
 * @note For cache line size, use ::logger::internal::kCacheLineSize from
 *       internal/cacheline.h which provides architecture-aware detection.
 * @note For CPU pause/yield instructions, use LOGGER_CPU_RELAX() from
 *       internal/platform.h which provides per-architecture implementations.
 */
#ifndef LOGGER_BACKEND_SPIN_COUNT
#define LOGGER_BACKEND_SPIN_COUNT 1000
#endif

#endif // LOGGER_CONFIG_H
