#ifndef LOGGER_ERROR_H
#define LOGGER_ERROR_H

#include "config.h"

#include <cstdint>
#include <cstdio>

namespace logger {

/**
 * @brief Error codes for non-fatal logger failures.
 *
 * Errors are reported only on the consumer side; producer remains non-blocking.
 */
enum class ErrorCode : std::uint8_t {
    None = 0,
    FileOpenFailed,
    WriteFailed,
    FlushFailed
};

/**
 * @brief Convert error code to a short string.
 */
inline const char *ErrorToString(ErrorCode code) noexcept {
    switch (code) {
    case ErrorCode::None:
        return "NONE";
    case ErrorCode::FileOpenFailed:
        return "FILE_OPEN_FAILED";
    case ErrorCode::WriteFailed:
        return "WRITE_FAILED";
    case ErrorCode::FlushFailed:
        return "FLUSH_FAILED";
    }
    return "UNKNOWN";
}

/**
 * @brief Report an error to stderr when diagnostics are enabled.
 *
 * Intended for consumer-side failures only.
 */
inline void ReportError(ErrorCode code, const char *context) noexcept {
#if LOGGER_ENABLE_STDERR_DIAGNOSTICS
    std::fprintf(stderr, "[LOGGER] %s: %s\n", context ? context : "error", ErrorToString(code));
#else
    (void)code;
    (void)context;
#endif
}

} // namespace logger

#endif // LOGGER_ERROR_H
