#include "../include/formatter.h"
#include "../include/level.h"
#include "../include/record.h"
#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace logger {

std::size_t TextFormatter::FormatRecord(const LogRecord &record, char *buffer, std::size_t capacity) {
    if (!buffer || capacity == 0) {
        return 0;
    }

    std::size_t pos = 0;

    auto appendf = [&](const char *fmt, ...) -> bool {
        if (pos >= capacity) {
            return false;
        }
        va_list args;
        va_start(args, fmt);
        int written = std::vsnprintf(buffer + pos, capacity - pos, fmt, args);
        va_end(args);
        if (written < 0) {
            buffer[pos] = '\0';
            return false;
        }
        if (static_cast<std::size_t>(written) >= (capacity - pos)) {
            pos = capacity - 1;
            buffer[pos] = '\0';
            return false;
        }
        pos += static_cast<std::size_t>(written);
        return true;
    };

    (void)appendf("[%llu] [%s]",
                  static_cast<unsigned long long>(record.timestamp),
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

    std::size_t msg_len = record.message_length;
    if (msg_len > 0 && pos < capacity - 1) {
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
