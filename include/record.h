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
    /*Core fields (always present/necessary)*/

    Level level;                                                                 // 1 byte
    internal::CachelinePad<sizeof(Level)> padding1;                              // (alignment)
    std::uint64_t timestamp;                                                     // 8 bytes (TSC or nanoseconds)
    std::size_t message_length;                                                  // 8 bytes
    internal::CachelinePad<sizeof(timestamp) + sizeof(message_length)> padding2; // (alignment)

    /*Optional fields (conditionally compiled)*/

#if LOGGER_ENABLE_THREAD_ID
    std::uint64_t thread_id; // 8 bytes
    internal::CachelinePad<sizeof(thread_id)> padding3;
#endif

#if LOGGER_ENABLE_SOURCE_LOCATION
    const char *file;                                      // pointer to __FILE__ (static string)
    internal::CachelinePad<sizeof(file)> padding_file;     // pad to cache line
    const char *function;                                  // pointer to __func__ (static string)
    internal::CachelinePad<sizeof(function)> padding_func; // pad to cache line
    std::int32_t line;                                     // line number from __LINE__
    internal::CachelinePad<sizeof(line)> padding_line;     // pad to cache line

#endif

    /*Message payload (fixed-size buffer)*/

    char message[LOGGER_MAX_MESSAGE_SIZE]; // N bytes from config.h

    /*Methods*/

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
        /*
        lets say our fmt was smth like || "hi your number is %d" then we need argument int so thats
        what args... is for and it will be expanded to int so that snprintf can use it
        */
        int result = std::snprintf(message, LOGGER_MAX_MESSAGE_SIZE, fmt, args...);
        if (result < 0) {
            message[0] = '\0';
            message_length = 0;
            return 0;
        }
        // Clamp to buffer size
        /*
            so say this string "hi your number is %d" and we pass 10 as argument so snprintf will
            return 18.LOGGER_MAX_MESSAGE_SIZE then we will clamp it to LOGGER_MAX_MESSAGE_SIZE - 1
        */
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

/* Compile-time validation */

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
