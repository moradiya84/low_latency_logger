/**
 * @file record.h
 * @brief Fixed-size log record structure for low-latency logger
 *
 * Defines the LogRecord struct that represents a single log entry.
 * Designed for zero-allocation on the hot path with cache-line alignment.
 *
 * RESPONSIBILITIES:
 * - Fixed-size, POD-like structure
 * - In-place message storage (no heap allocation)
 * - Cache-line aligned for efficient ring buffer storage
 *
 * ANTI-RESPONSIBILITIES:
 * - No I/O
 * - No virtual functions
 * - No ownership of ring buffer
 */

#ifndef LOGGER_RECORD_H
#define LOGGER_RECORD_H

#include "../internal/cacheline.h"
#include "config.h"
#include "level.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace logger {

/**
 * @brief Fixed-size log record structure
 *
 * This struct is designed to be pushed into an SPSC ring buffer with
 * minimal latency. All data is stored inline with no heap allocation.
 *
 * Layout is cache-line aligned to prevent false sharing in the ring buffer.
 */
struct alignas(internal::kCacheLineSize) LogRecord {
    // === Core fields (always present) ===

    Level level;                // 1 byte
    std::uint8_t padding1[7];   // 7 bytes (alignment)
    std::uint64_t timestamp;    // 8 bytes (TSC or nanoseconds)
    std::size_t message_length; // 8 bytes

    // === Optional fields (conditionally compiled) ===

#if LOGGER_ENABLE_THREAD_ID
    std::uint64_t thread_id; // 8 bytes
#endif

#if LOGGER_ENABLE_SOURCE_LOCATION
    const char *file;         // 8 bytes (pointer to static string)
    const char *function;     // 8 bytes (pointer to static string)
    std::int32_t line;        // 4 bytes
    std::uint8_t padding2[4]; // 4 bytes (alignment)
#endif

    // === Message payload (fixed-size buffer) ===

    char message[LOGGER_MAX_MESSAGE_SIZE]; // N bytes from config.h

    // === Methods ===

    /**
     * @brief Set the message content (copy from null-terminated string)
     * @param msg Null-terminated source string
     * @return Number of bytes written (excluding null terminator)
     */
    std::size_t SetMessage(const char *msg) noexcept {
        if (!msg) {
            message[0] = '\0';
            message_length = 0;
            return 0;
        }
        // Safe copy with truncation
        std::size_t len = std::strlen(msg);
        if (len >= LOGGER_MAX_MESSAGE_SIZE) {
            len = LOGGER_MAX_MESSAGE_SIZE - 1;
        }
        std::memcpy(message, msg, len);
        message[len] = '\0';
        message_length = len;
        return len;
    }

    /**
     * @brief Set the message content with explicit length
     * @param msg Source buffer
     * @param len Number of bytes to copy
     * @return Number of bytes written
     */
    std::size_t SetMessage(const char *msg, std::size_t len) noexcept {
        if (!msg || len == 0) {
            message[0] = '\0';
            message_length = 0;
            return 0;
        }
        if (len >= LOGGER_MAX_MESSAGE_SIZE) {
            len = LOGGER_MAX_MESSAGE_SIZE - 1;
        }
        std::memcpy(message, msg, len);
        message[len] = '\0';
        message_length = len;
        return len;
    }

    /**
     * @brief Format a message using printf-style formatting
     * @param fmt Format string
     * @param ... Format arguments
     * @return Number of bytes written (excluding null terminator)
     *
     * Note: Uses snprintf, which is safe but may have performance overhead.
     * For absolute minimum latency, prefer SetMessage() with pre-formatted strings.
     */
    template <typename... Args>
    std::size_t FormatMessage(const char *fmt, Args... args) noexcept {
        if (!fmt) {
            message[0] = '\0';
            message_length = 0;
            return 0;
        }
        // snprintf returns the number of characters that would have been written
        // (excluding null terminator), or negative on error
        int result = std::snprintf(message, LOGGER_MAX_MESSAGE_SIZE, fmt, args...);
        if (result < 0) {
            message[0] = '\0';
            message_length = 0;
            return 0;
        }
        // Clamp to buffer size
        message_length = (static_cast<std::size_t>(result) < LOGGER_MAX_MESSAGE_SIZE)
                             ? static_cast<std::size_t>(result)
                             : LOGGER_MAX_MESSAGE_SIZE - 1;
        return message_length;
    }

#if LOGGER_ENABLE_SOURCE_LOCATION
    /**
     * @brief Set source location information
     * @param file_name Pointer to __FILE__ (static storage)
     * @param line_number Line number from __LINE__
     * @param func_name Pointer to __func__ (static storage)
     */
    void SetSourceLocation(const char *file_name, int line_number, const char *func_name) noexcept {
        file = file_name;
        line = line_number;
        function = func_name;
    }
#endif
};

// === Compile-time validation ===

// Ensure LogRecord is cache-line aligned
static_assert(alignof(LogRecord) >= internal::kCacheLineSize,
              "LogRecord must be cache-line aligned");

// Ensure LogRecord fits reasonable size (warn if too large)
static_assert(sizeof(LogRecord) <= 4096,
              "LogRecord is very large, consider reducing LOGGER_MAX_MESSAGE_SIZE");

// Ensure nothrow destructible (required for ring buffer)
static_assert(std::is_nothrow_destructible_v<LogRecord>,
              "LogRecord must be nothrow destructible");

// Ensure nothrow move constructible (required for ring buffer)
static_assert(std::is_nothrow_move_constructible_v<LogRecord>,
              "LogRecord must be nothrow move constructible");

} // namespace logger

#endif // LOGGER_RECORD_H
