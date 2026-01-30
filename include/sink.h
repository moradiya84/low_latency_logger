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

/**
 * @brief File-backed sink (append-only)
 *
 * Uses stdio buffering; called only from the consumer thread.
 */
class alignas(internal::kCacheLineSize) FileSink final : public Sink {
  public:
    explicit FileSink(const char *path, const char *mode = "ab") noexcept;
    /* once base class deconstructor is virtual all of it's child classes are virtual as well so it runs even when compiler deletes sink obj.
       override keyword just to verify at compile time that base class also have virtual deconstructor
      override is not neccasary for deconstructor it will always overide virtual deconstructor from parent class
    */
    ~FileSink() override;
    // same here writting override is not ne
    void Write(const char *data, std::size_t len) override;
    void Flush() override;

  private:
    std::FILE *file_;
};

/**
 * @brief Console sink (stdout or stderr)
 */
class alignas(internal::kCacheLineSize) ConsoleSink final : public Sink {
  public:
    enum class Stream : std::uint8_t { Stdout,
                                       Stderr };

    explicit ConsoleSink(Stream stream = Stream::Stdout) noexcept;

    void Write(const char *data, std::size_t len) override;
    void Flush() override;

  private:
    std::FILE *stream_;
};

/**
 * @brief Null sink (drops all output)
 */
class alignas(internal::kCacheLineSize) NullSink final : public Sink {
  public:
    void Write(const char *, std::size_t) override;
    void Flush() override;
};
} // namespace logger
#endif // LOGGER_SINK_H
