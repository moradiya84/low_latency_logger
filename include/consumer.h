/**
 * @file consumer.h
 * @brief Background consumer thread for the low-latency logger
 *
 * Defines the Consumer class which runs in a background thread to drain
 * the ring buffer, format format log records, and write them to the sink.
 *
 * RESPONSIBILITIES:
 * - Drain SpscRingBuffer<LogRecord>
 * - Format records using Formatter
 * - Write output using Sink
 * - Manage background thread lifecycle (Start/Stop)
 * - Implement hybrid spin/sleep wait strategy for low latency
 *
 * ANTI-RESPONSIBILITIES:
 * - No ownership of ring buffer (reference only)
 * - No direct file I/O (delegated to Sink)
 * - No formatting logic (delegated to Formatter)
 */

#ifndef LOGGER_CONSUMER_H
#define LOGGER_CONSUMER_H

#include "../internal/platform.h"
#include "../internal/ring_buffer.h"
#include "config.h"
#include "formatter.h"
#include "record.h"
#include "sink.h"

#include <atomic>
#include <chrono>
#include <cstddef>
#include <thread>
#include <type_traits>

namespace logger {

/**
 * @brief Background log consumer
 *
 * This class wraps the background worker thread that continuously polls
 * the ring buffer for new log records. It uses a hybrid wait strategy:
 * 1. Busy spin for LOGGER_BACKEND_SPIN_COUNT iterations (low latency)
 * 2. Sleep for a short duration (to save CPU when idle)
 *
 * @tparam Capacity Size of the ring buffer (must match ring buffer's capacity)
 */
template <std::size_t Capacity>
class Consumer {
  public:
    /**
     * @brief Construct a new Consumer
     *
     * @param ring_buffer Reference to the shared ring buffer
     * @param formatter Reference to the formatter implementation
     * @param sink Reference to the sink implementation
     */
    Consumer(internal::SpscRingBuffer<LogRecord, Capacity> &ring_buffer, Formatter &formatter, Sink &sink)
        : ring_buffer_(ring_buffer), formatter_(formatter), sink_(sink), is_running_(false) {}

    /**
     * @brief Destructor
     *
     * Stops the consumer thread (if running) and joins it.
     */
    ~Consumer() {
        Stop();
    }

    // Non-copyable, Non-movable
    Consumer(const Consumer &) = delete;
    Consumer &operator=(const Consumer &) = delete;
    Consumer(Consumer &&) = delete;
    Consumer &operator=(Consumer &&) = delete;

    /**
     * @brief Start the consumer thread
     *
     * Spawns the background thread if it's not already running.
     * Does nothing if already running.
     */
    void Start() {
        bool expected = false;
        /*
            is_running_ is std::atomic<bool> it replaces mutex lock and kinda cheap
            here we want to make sure one thread starts one time only.

            compare_exchange_strong is a compare thread's exchange with current exchange(which is false) so if thread's
                exchange is false then it will exchange with true and start the thread.else it will leave is_running_ unchanged.

            so this condition will be called only by first call of any thread.
        */
        if (is_running_.compare_exchange_strong(expected, true)) {
            thread_ = std::thread(&Consumer::Loop, this);
        }
    }

    /**
     * @brief Stop the consumer thread
     *
     * Signals the thread to stop and joins it.
     * This blocks until the thread terminates.
     */
    void Stop() {
        bool expected = true;
        /*
            same as before here we expect thread to be running and then we stops it
        */
        if (is_running_.compare_exchange_strong(expected, false)) {
            if (thread_.joinable()) {
                thread_.join();
            }
        }
    }

    /**
     * @brief Check if the consumer is running
     * @return true if running, false otherwise
     */
    bool IsRunning() const {
        return is_running_.load(std::memory_order_relaxed);
    }

  private:
    /**
     * @brief Main loop for the consumer thread
     */
    void Loop() {
        // Local buffer for formatting messages.
        // Size = LOGGER_MAX_MESSAGE_SIZE + overhead for:
        //   - Timestamp (~30 bytes)
        //   - Level string (~10 bytes)
        //   - Thread ID (~20 bytes)
        //   - Source location file/line/function (~150 bytes)
        //   - Brackets, spaces, newline (~46 bytes)
        // Total overhead: ~256 bytes
        static constexpr std::size_t kFormattingOverhead = 256;
        static constexpr std::size_t kScratchBufferSize = LOGGER_MAX_MESSAGE_SIZE + kFormattingOverhead;
        char scratch_buffer[kScratchBufferSize];
        LogRecord record;

        while (is_running_.load(std::memory_order_relaxed)) {
            // fast path: consume available items
            if (ring_buffer_.TryPop(record)) {
                std::size_t len = formatter_.FormatRecord(record, scratch_buffer, sizeof(scratch_buffer));
                sink_.Write(scratch_buffer, len);
                continue; // Immediately check for more
            }

            // empty path: flush and wait
            sink_.Flush();

            // Hybrid Wait Strategy:
            // 1. Spin-wait for recent activity (low latency)
            bool found_activity = false;
            for (int i = 0; i < LOGGER_BACKEND_SPIN_COUNT; ++i) {
                LOGGER_CPU_RELAX();
                // Check stop signal during spin to remain responsive
                if (!is_running_.load(std::memory_order_relaxed)) {
                    goto shutdown;
                }
                /*
                    if this try pop is successful then it also deletes that data from ring buffer and that data is now only available
                    in record here  which is local to this thread and loop as well from line 146.


                    so we have to write same logic as starting of loop in if(found_activity) block.
                */
                if (ring_buffer_.TryPop(record)) {
                    found_activity = true;
                    break;
                }
            }

            if (found_activity) {
                std::size_t len = formatter_.FormatRecord(record, scratch_buffer, sizeof(scratch_buffer));
                sink_.Write(scratch_buffer, len);
            } else {
                // 2. Sleep to save CPU if no activity for a while
                // Check runs flag one last time before sleeping
                if (is_running_.load(std::memory_order_relaxed)) {
                    std::this_thread::sleep_for(std::chrono::microseconds(500));
                }
            }
        }

    shutdown:
        // Ensure everything is flushed before exit
        sink_.Flush();
    }

    internal::SpscRingBuffer<LogRecord, Capacity> &ring_buffer_;
    Formatter &formatter_;
    Sink &sink_;

    std::atomic<bool> is_running_;
    std::thread thread_;
};

} // namespace logger

#endif // LOGGER_CONSUMER_H
