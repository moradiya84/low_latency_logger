/**
 * @file level.h
 * @brief Log severity levels for low-latency logger
 *
 * Defines strongly-typed log levels with constexpr string conversion.
 * No runtime filtering logic, formatting, or I/O.
 */

#ifndef LOGGER_LEVEL_H
#define LOGGER_LEVEL_H

#include <cstddef>
#include <cstdint>

namespace logger {

/**
 * @brief Log severity levels in ascending order of severity
 *
 * Trace < Debug < Info < Warn < Error < Fatal
 */
enum class Level : std::uint8_t {
    Trace = 0, // Fine-grained debugging (very verbose)
    Debug = 1, // Debugging information
    Info = 2,  // General informational messages
    Warn = 3,  // Warning conditions
    Error = 4, // Error conditions
    Fatal = 5  // Critical errors, program may terminate
};

/**
 * @brief Convert Level to string (constexpr, no allocation)
 * @param level The log level
 * @return Null-terminated string literal
 */
constexpr const char *LevelToString(Level level) noexcept {
    switch (level) {
    case Level::Trace:
        return "TRACE";
    case Level::Debug:
        return "DEBUG";
    case Level::Info:
        return "INFO";
    case Level::Warn:
        return "WARN";
    case Level::Error:
        return "ERROR";
    case Level::Fatal:
        return "FATAL";
    }
    return "UNKNOWN";
}

/**
 * @brief Convert Level to single character (for compact output)
 * @param level The log level
 * @return Single character representing the level
 */
constexpr char LevelToChar(Level level) noexcept {
    switch (level) {
    case Level::Trace:
        return 'T';
    case Level::Debug:
        return 'D';
    case Level::Info:
        return 'I';
    case Level::Warn:
        return 'W';
    case Level::Error:
        return 'E';
    case Level::Fatal:
        return 'F';
    }
    return '?';
}

/**
 * @brief Get the underlying integer value of a Level
 * @param level The log level
 * @return Integer value (0-5)
 */
constexpr std::uint8_t LevelToInt(Level level) noexcept {
    return static_cast<std::uint8_t>(level);
}

/**
 * @brief Check if a level should be logged given a minimum threshold
 * @param level The level of the message
 * @param min_level The minimum level to log
 * @return true if level >= min_level
 */
constexpr bool ShouldLog(Level level, Level min_level) noexcept {
    return static_cast<std::uint8_t>(level) >= static_cast<std::uint8_t>(min_level);
}

// Compile-time level count
/*
    klevelcount will give us the number of levels we have in our enum
*/
constexpr std::size_t kLevelCount = 6;

} // namespace logger

#endif // LOGGER_LEVEL_H
