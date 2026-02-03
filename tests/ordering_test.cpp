#include "../include/formatter.h"
#include "../include/level.h"
#include "../include/record.h"

#include <cassert>
#include <cstddef>
#include <cstring>

int main() {
    logger::TextFormatter formatter;

    // Build a deterministic record for formatting order checks.
    logger::LogRecord record{};
    record.level = logger::Level::Info;
    record.timestamp = 0; // TSC=0 should convert to 0ns.
    record.SetMessage("hello");

#if LOGGER_ENABLE_THREAD_ID
    record.thread_id = 42;
#endif

#if LOGGER_ENABLE_SOURCE_LOCATION
    record.file = "file.cc";
    record.function = "func";
    record.line = 7;
#endif

    char buffer[256];
    const std::size_t written = formatter.FormatRecord(record, buffer, sizeof(buffer));
    assert(written > 0);

    // Basic prefix order: timestamp then level.
    assert(std::strncmp(buffer, "[0] [INFO]", 10) == 0);

#if LOGGER_ENABLE_THREAD_ID
    // Thread id should appear after level when enabled.
    assert(std::strstr(buffer, "[tid=42]") != nullptr);
#endif

#if LOGGER_ENABLE_SOURCE_LOCATION
    // Source location should appear when enabled.
    assert(std::strstr(buffer, "file.cc:7 func") != nullptr);
#endif

    // Message should be present near the end.
    assert(std::strstr(buffer, "hello") != nullptr);

    // Line-delimited output should end with newline.
    assert(buffer[written - 1] == '\n');

    return 0;
}
