/**
 * @file logger.h
 * @brief Main logger interface for low-latency asynchronous logging
 *
 * Defines the Logger class which provides the public API for logging.
 * This is the primary interface that application code uses to log messages.
 *
 * RESPONSIBILITIES:
 * - Provide Log() methods for different log levels
 * - Capture timestamps, thread IDs, and source locations
 * - Push log records to the ring buffer (non-blocking)
 * - Manage ring buffer, formatter, sink, and consumer lifecycle
 * - Handle backpressure (drop logs when buffer is full)
 *
 * ANTI-RESPONSIBILITIES:
 * - No formatting logic (delegated to Formatter)
 * - No I/O operations (delegated to Sink)
 * - No background thread management (delegated to Consumer)
 */

#ifndef LOGGER_LOGGER_H
#define LOGGER_LOGGER_H

#include "../internal/platform.h"
#include "../internal/ring_buffer.h"
#include "config.h"
#include "consumer.h"
#include "formatter.h"
#include "level.h"
#include "record.h"
#include "sink.h"

#include <atomic>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <functional>
#include <thread>

namespace logger {

/**
 * @brief Result of a log operation
 */
enum class LogResult {
    Success,    // Log record was successfully enqueued
    BufferFull, // Ring buffer is full, log was dropped
    Error       // Other error (should not happen in normal operation)
};

/**
 * @brief Main logger class
 *
 * This is the primary interface for logging. It provides methods to log
 * messages at different severity levels. All logging operations are
 * non-blocking and lock-free on the hot path.
 *
 * The logger uses a single-producer, single-consumer ring buffer internally.
 * Only one producer thread may call Log() concurrently.
 * One consumer thread drains the buffer.
 *
 * @tparam Capacity Size of the ring buffer (must be a power of 2)
 */
template <std::size_t Capacity>
class Logger {
  public:
    /**
     * @brief Construct a new Logger
     *
     * @param formatter Reference to the formatter implementation
     * @param sink Reference to the sink implementation
     *
     * Note: The formatter and sink must outlive the Logger.
     */
    Logger(Formatter &formatter, Sink &sink) : consumer_(ring_buffer_, formatter, sink) {
        // Ring buffer is default-constructed (empty)
        // Consumer is constructed but not started
    }

    /**
     * @brief Destructor
     *
     * Stops the consumer thread and ensures all logs are flushed.
     */
    ~Logger() {
        Stop();
    }

    // Non-copyable, Non-movable
    Logger(const Logger &) = delete;
    Logger &operator=(const Logger &) = delete;
    Logger(Logger &&) = delete;
    Logger &operator=(Logger &&) = delete;

    /**
     * @brief Start the background consumer thread
     *
     * This must be called before logging. The consumer thread will
     * continuously drain the ring buffer and write logs to the sink.
     */
    void Start() {
        consumer_.Start();
    }

    /**
     * @brief Stop the background consumer thread
     *
     * Signals the consumer to stop and waits for it to finish.
     * This will flush any remaining logs in the buffer.
     */
    void Stop() {
        consumer_.Stop();
    }

    /**
     * @brief Check if the logger is running
     * @return true if the consumer thread is running
     */
    bool IsRunning() const {
        return consumer_.IsRunning();
    }

    /**
     * @brief Log a message at the specified level
     *
     * @param level Log severity level
     * @param message Null-terminated message string
     * @return LogResult indicating success or failure
        Basically what Log() does is it prepares the record, sets the message, and pushes the record to the ring buffer.
     */
    LOGGER_FORCE_INLINE LogResult Log(Level level, const char *message) noexcept {
        return LogImpl(level, message, nullptr, 0, nullptr);
    }

    /**
     * @brief Log a message with source location
     *
     * @param level Log severity level
     * @param message Null-terminated message string
     * @param file Source file name (typically __FILE__)
     * @param line Line number (typically __LINE__)
     * @param function Function name (typically __func__)
     * @return LogResult indicating success or failure
     */
    LOGGER_FORCE_INLINE LogResult Log(Level level, const char *message, const char *file, int line, const char *function) noexcept {
#if LOGGER_ENABLE_SOURCE_LOCATION
        return LogImpl(level, message, file, line, function);
#else
        // Source location disabled, ignore parameters
        (void)file;
        (void)line;
        (void)function;
        return LogImpl(level, message, nullptr, 0, nullptr);
#endif
    }

    /**
     * @brief Log a formatted message using printf-style formatting
     *
     * @param level Log severity level
     * @param fmt Format string
     * @param args Format arguments
     * @return LogResult indicating success or failure
     */
    template <typename... Args>
    LOGGER_FORCE_INLINE LogResult LogFormat(Level level, const char *fmt, Args... args) noexcept {
        LogRecord record;
        if (!PrepareRecord(record, level)) {
            return LogResult::Error;
        }
        record.FormatMessage(fmt, args...);
        return PushRecord(record);
    }

    /**
     * @brief Log a formatted message with source location
     *
     * @param level Log severity level
     * @param file Source file name
     * @param line Line number
     * @param function Function name
     * @param fmt Format string
     * @param args Format arguments
     * @return LogResult indicating success or failure
     */
    template <typename... Args>
    LOGGER_FORCE_INLINE LogResult LogFormat(Level level, const char *file, int line, const char *function,
                                            const char *fmt, Args... args) noexcept {
        LogRecord record;
        if (!PrepareRecord(record, level)) {
            return LogResult::Error;
        }
#if LOGGER_ENABLE_SOURCE_LOCATION
        record.SetSourceLocation(file, line, function);
#else
        (void)file;
        (void)line;
        (void)function;
#endif
        record.FormatMessage(fmt, args...);
        return PushRecord(record);
    }

    // Convenience methods for each log level

    LOGGER_FORCE_INLINE LogResult Trace(const char *message) noexcept {
        return Log(Level::Trace, message);
    }

    LOGGER_FORCE_INLINE LogResult Debug(const char *message) noexcept {
        return Log(Level::Debug, message);
    }

    LOGGER_FORCE_INLINE LogResult Info(const char *message) noexcept {
        return Log(Level::Info, message);
    }

    LOGGER_FORCE_INLINE LogResult Warn(const char *message) noexcept {
        return Log(Level::Warn, message);
    }

    LOGGER_FORCE_INLINE LogResult Error(const char *message) noexcept {
        return Log(Level::Error, message);
    }

    LOGGER_FORCE_INLINE LogResult Fatal(const char *message) noexcept {
        return Log(Level::Fatal, message);
    }

    // Convenience methods with source location

    LOGGER_FORCE_INLINE LogResult Trace(const char *message, const char *file, int line,
                                        const char *function) noexcept {
        return Log(Level::Trace, message, file, line, function);
    }

    LOGGER_FORCE_INLINE LogResult Debug(const char *message, const char *file, int line,
                                        const char *function) noexcept {
        return Log(Level::Debug, message, file, line, function);
    }

    LOGGER_FORCE_INLINE LogResult Info(const char *message, const char *file, int line,
                                       const char *function) noexcept {
        return Log(Level::Info, message, file, line, function);
    }

    LOGGER_FORCE_INLINE LogResult Warn(const char *message, const char *file, int line,
                                       const char *function) noexcept {
        return Log(Level::Warn, message, file, line, function);
    }

    LOGGER_FORCE_INLINE LogResult Error(const char *message, const char *file, int line,
                                        const char *function) noexcept {
        return Log(Level::Error, message, file, line, function);
    }

    LOGGER_FORCE_INLINE LogResult Fatal(const char *message, const char *file, int line,
                                        const char *function) noexcept {
        return Log(Level::Fatal, message, file, line, function);
    }

    /**
     * @brief Get approximate number of pending log records
     * @return Number of records in the buffer (may be stale)
     */
    std::size_t PendingCount() const noexcept {
        return ring_buffer_.Size();
    }

    /**
     * @brief Check if the ring buffer is full
     * @return true if buffer is full
     */
    bool IsBufferFull() const noexcept {
        return ring_buffer_.Full();
    }

  private:
    /**
     * @brief Internal implementation of Log()
        Basically what LogImpl() does is it checks if the message is not null, prepares the record, sets the message, sets the source location (if enabled), and pushes the record to the ring buffer.
     */
    LOGGER_FORCE_INLINE LogResult LogImpl(Level level, const char *message, const char *file, int line,
                                          const char *function) noexcept {
        if (LOGGER_UNLIKELY(!message)) {
            return LogResult::Error;
        }

        LogRecord record;
        if (!PrepareRecord(record, level)) {
            return LogResult::Error;
        }

        record.SetMessage(message);

#if LOGGER_ENABLE_SOURCE_LOCATION
        if (file && function) {
            record.SetSourceLocation(file, line, function);
        }
#endif
        return PushRecord(record);
    }

    /**
     * @brief Prepare a log record with common fields (timestamp, thread ID)
     * @param record Reference to the record to prepare
     * @param level Log level
     * @return true on success, false on error
     */
    LOGGER_FORCE_INLINE bool PrepareRecord(LogRecord &record, Level level) noexcept {
        record.level = level;

        // Capture timestamp (using TSC for lowest latency)
        record.timestamp = internal::ReadTsc();

#if LOGGER_ENABLE_SOURCE_LOCATION
        record.file = nullptr;
        record.function = nullptr;
        record.line = 0;
#endif

#if LOGGER_ENABLE_THREAD_ID
        // Get thread ID
        // Using std::hash<std::thread::id> for portability
        // Note: This is not the OS thread ID, but a unique identifier
        record.thread_id = std::hash<std::thread::id>{}(std::this_thread::get_id());
#endif

        return true;
    }

    /**
     * @brief Push a prepared record to the ring buffer
     * @param record The record to push
     * @return LogResult indicating success or failure
     */
    LOGGER_FORCE_INLINE LogResult PushRecord(LogRecord &record) noexcept {
        // Try to push the record (non-blocking)
        if (LOGGER_LIKELY(ring_buffer_.TryPush(record))) {
            return LogResult::Success;
        }

        // Buffer is full - drop the log
        // This is the backpressure strategy: drop when full
#if LOGGER_ENABLE_STDERR_DIAGNOSTICS
        // Optionally report dropped logs (only if enabled)
        // Note: This uses stderr, which may have some overhead
        // this is to know how many logs are dropped when buffer is full
        static std::atomic<std::size_t> dropped_count{0};
        //.fetch_add(x,memory_order) return old value
        std::size_t dropped = dropped_count.fetch_add(1, std::memory_order_relaxed) + 1;
        if (dropped == 1 || (dropped % 1000 == 0)) {
            // Only log first drop and every 1000th drop to avoid spam
            std::fprintf(stderr, "[LOGGER] Warning: Log buffer full, dropped %zu log(s)\n", dropped);
        }
#endif
        return LogResult::BufferFull;
    }

    internal::SpscRingBuffer<LogRecord, Capacity> ring_buffer_;
    Consumer<Capacity> consumer_;
};

} // namespace logger

#endif // LOGGER_LOGGER_H
