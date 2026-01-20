/**
 * @file formatter.h
 * @brief Logic for converting LogRecord to string output
 *
 * Defines the Formatter class which handles the formatting of log records
 * into a character buffer.
 *
 * RESPONSIBILITIES:
 * - Format LogRecord into a buffer (bytes)
 * - Handle timestamp formatting (TSC to nanos/human readable)
 * - Handle log level string conversion
 *
 * ANTI-RESPONSIBILITIES:
 * - No I/O (sink's job)
 * - No memory allocation
 */

#ifndef LOGGER_FORMATTER_H
#define LOGGER_FORMATTER_H

#include "record.h"

#include <cstddef>

namespace logger {

/**
 * @brief Formats LogRecord objects into text
 *
 * Designed to be used by the Consumer thread.
 * Not necessarily thread-safe (Consumer owns it exclusively).
 */
class Formatter {
  public:
    virtual ~Formatter() = default;

    /**
     * @brief Serializes the entire log record into a buffer
     * @param record The log record to serialize
     * @param buffer Destination buffer
     * @param capacity Size of the destination buffer
     * @return Number of bytes written
     *
     * Note: This is distinct from LogRecord::FormatMessage.
     * - LogRecord::FormatMessage() runs on PRODUCER to populate the payload.
     * - Formatter::FormatRecord() runs on CONSUMER to serialize the full line (timestamp+level+payload).
     */
    virtual std::size_t FormatRecord(const LogRecord &record, char *buffer, std::size_t capacity) = 0;

  protected:
    Formatter() = default;
};

/**
 * @brief Default text formatter implementation
 */
class TextFormatter : public Formatter {
  public:
    std::size_t FormatRecord(const LogRecord &record, char *buffer, std::size_t capacity) override;
};

} // namespace logger

#endif // LOGGER_FORMATTER_H
