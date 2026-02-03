#include "../include/level.h"
#include "../include/record.h"
#include "../internal/clock.h"

#include <cstddef>
#include <cstdio>
#include <cstring>

namespace logger {

/**
 * @brief Encode a LogRecord into a text line.
 *
 * Produces one log line with timestamp, level, optional metadata, and message.
 * Uses the caller-provided buffer to avoid heap allocation.
 */
std::size_t EncodeTextRecord(const LogRecord &record, char *buffer, std::size_t capacity) noexcept {
    if (!buffer || capacity == 0) {
        return 0;
    }
    std::size_t pos = 0;

    // Append formatted text into the buffer without allocating.
    auto appendf = [&](const char *fmt, auto... args) -> bool {
        if (pos >= capacity) {
            return false;
        }
        int written = std::snprintf(buffer + pos, capacity - pos, fmt, args...);
        if (written < 0) {
            buffer[pos] = '\0';
            return false;
        }
        // means we coun't write entire log.
        if (static_cast<std::size_t>(written) >= (capacity - pos)) {
            pos = capacity - 1;
            buffer[pos] = '\0';
            return false;
        }
        pos += static_cast<std::size_t>(written);
        return true;
    };

    // Convert TSC ticks to nanoseconds for human-readable timestamps.
    const std::uint64_t timestamp_ns = internal::TscToNanoseconds(record.timestamp);
    (void)appendf("[%llu] [%s]",
                  static_cast<unsigned long long>(timestamp_ns),
                  LevelToString(record.level));

#if LOGGER_ENABLE_THREAD_ID
    (void)appendf(" [tid=%llu]",
                  static_cast<unsigned long long>(record.thread_id));
#endif

#if LOGGER_ENABLE_SOURCE_LOCATION
    if (record.file && record.function) {
        (void)appendf(" %s:%d %s", record.file, record.line, record.function);
    }
#endif

    if (pos < capacity - 1) {
        buffer[pos++] = ' ';
    }

    // we are not using appendf because appendf is used for formatting + write and for string ends with /n
    // our message might have multiple strings so it might have /n,/0 in middle as well.
    // Message length recorded at write time (may be truncated below).
    std::size_t msg_len = record.message_length;
    if (msg_len > 0 && pos < capacity - 1) {
        // Available space for the message payload (leave room for terminator).
        std::size_t available = capacity - 1 - pos;
        if (msg_len > available) {
            msg_len = available;
        }
        std::memcpy(buffer + pos, record.message, msg_len);
        pos += msg_len;
    }

    if (pos < capacity - 1) {
        buffer[pos++] = '\n';
    }

    buffer[pos] = '\0';
    return pos;
}

} // namespace logger
