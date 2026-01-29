/**
 * @file sink.h
 * @brief Abstract interface for log output destinations
 *
 * Defines the Sink interface and a few minimal concrete implementations.
 *
 * RESPONSIBILITIES:
 * - Define Write() and Flush() interface
 * - Virtual destructor for proper cleanup
 *
 * ANTI-RESPONSIBILITIES:
 * - No formatting (formatter's job)
 * - No buffering logic (sink implementation decides)
 * - Keep I/O off the producer hot path (consumer thread only)
 */

#ifndef LOGGER_SINK_H
#define LOGGER_SINK_H

#include "../internal/cacheline.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>

namespace logger {

/**
 * @brief Abstract interface for log output destinations
 *
 * Implementations include:
 * - FileSink: writes to a file
 * - ConsoleSink: writes to stdout/stderr
 * - NullSink: discards all output (for benchmarking)
 *
 * Called ONLY from the consumer thread (never from hot path).
 */
class Sink {
  public:
    /*
      deconstructor is virtual so that derived class destructor is called before base class destructor
      if we don't make it virtual then only base class destructor will be called and derived class destructor will not be called
    */
    virtual ~Sink() = default;

    /**
     * @brief Write formatted log data to the output
     * @param data Pointer to the formatted log data
     * @param len Number of bytes to write
     *
     * Called by the consumer thread after formatting.
     * Implementation may buffer internally.
     */
    virtual void Write(const char *data, std::size_t len) = 0;

    /**
     * @brief Flush any buffered data to the underlying output
     *
     * Called periodically by the consumer or on shutdown.
     * Must ensure all previously written data is persisted.
     */
    virtual void Flush() = 0;

    // Non-copyable, non-movable
    Sink(const Sink &) = delete;
    Sink &operator=(const Sink &) = delete;
    Sink(Sink &&) = delete;
    Sink &operator=(Sink &&) = delete;

  protected:
    // Only derived classes can construct
    Sink() = default;
};
} // namespace logger
#endif // LOGGER_SINK_H