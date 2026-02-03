/**
 * @file ring_buffer.h
 * @brief Lock-free Single-Producer/Single-Consumer (SPSC) Ring Buffer
 *
 * This header implements a low-latency, lock-free ring buffer for use as
 * the core transport mechanism in an asynchronous logger. It guarantees
 * deterministic latency and strict memory ordering constraints.
 *
 * Design Contract:
 * - Single Producer, Single Consumer ONLY.
 * - Fixed capacity (power of 2).
 * - No dynamic allocation after construction.
 * - No exceptions.
 * - No locks.
 */

#ifndef LOGGER_INTERNAL_RING_BUFFER_H
#define LOGGER_INTERNAL_RING_BUFFER_H

#include "cacheline.h"
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <new>
#include <type_traits>
#include <utility>

namespace logger {
namespace internal {

/**
 * @brief Lock-free Single-Producer/Single-Consumer Ring Buffer
 *
 * @tparam T Element type. Must be nothrow destructible and nothrow move/copy constructible.
 * @tparam Capacity Buffer capacity. Must be a power of 2 and greater than 1.
 */
template <typename T, size_t Capacity>
class SpscRingBuffer {
  public:
    /*Compile-Time Constants and Validation*/

    static constexpr size_t kCapacity = Capacity;
    static constexpr size_t kMask = Capacity - 1;

    // Validate Capacity constraints
    static_assert(Capacity > 1,
                  "Capacity must be greater than one");
    static_assert((Capacity & (Capacity - 1)) == 0,
                  "Capacity must be a power of two");

    // Validate Element Type constraints
    static_assert(std::is_nothrow_destructible_v<T>,
                  "T must be nothrow destructible");
    // We require either move or copy to be nothrow to guarantee no-throw operation
    static_assert(std::is_nothrow_move_constructible_v<T> || std::is_nothrow_copy_constructible_v<T>,
                  "T must be move or copy constructible");

    /*Construction / Destruction*/

    /**
     * @brief Constructs an empty ring buffer.
     *
     * Establishes a fully usable empty buffer state.
     * No dynamic memory allocation.
     * No thread creation.
     */
    SpscRingBuffer() noexcept {
        // Compile-time layout checks (type is complete at instantiation).
        using Self = SpscRingBuffer<T, Capacity>;
        static_assert(offsetof(Self, read_index_) - offsetof(Self, write_index_) >= kCacheLineSize,
                      "Producer and Consumer indices must be on separate cache lines");
        static_assert(offsetof(Self, storage_) - offsetof(Self, read_index_) >= kCacheLineSize,
                      "Storage must be on a separate cache line from indices");
        static_assert(offsetof(Self, storage_) - offsetof(Self, write_index_) >= kCacheLineSize,
                      "Storage must be on a separate cache line from indices");
        // Atomic indices initialized to 0 by member initializers.
        // Storage is uninitialized raw memory.
    }

    /**
     * @brief Destroys the ring buffer.
     *
     * Does NOT drain the buffer. Caller is responsible for lifetime coordination
     * ensuring the consumer has processed all items if strictly required,
     * though normally the buffer is just abandoned.
     * No synchronization performed.
     */
    ~SpscRingBuffer() {
        // Intentionally does not destroy remaining elements.
        // See Requirement 22: "Destruction does not drain the buffer".
    }

    // Object semantics: Non-copyable, Non-movable
    SpscRingBuffer(const SpscRingBuffer &) = delete;
    SpscRingBuffer &operator=(const SpscRingBuffer &) = delete;
    SpscRingBuffer(SpscRingBuffer &&) = delete;
    SpscRingBuffer &operator=(SpscRingBuffer &&) = delete;

    /*Producer Operations*/

    /**
     * @brief Attempts to push an element into the buffer.
     *
     * @param element The element to copy.
     * @return true if successful, false if full.
     */
    bool TryPush(const T &element) noexcept {
        /*
            memory_order specifies how memory is ordered around atomic operations for both atomic and non-atomic memory access.

            memory_order_relaxed: no ordering constraints are applied
            why is it used here because only thread accessing/writing data is producer so no need to order memory access

            memory_order_acquire: acquire ordering constraint
            why is it used here because consumer thread is reading data so it needs to see the read progress

            while pushing data when we are trying to load write_idx which is producer's index so there is no need to order memory access
            but when we are trying to load read_idx which is consumer's index so there is need to order memory access
        */

        // Load indices
        // Relaxed load for producer's own index (exclusive ownership)
        const size_t write_idx = write_index_.value.load(std::memory_order_relaxed);
        // Acquire load for consumer's index to ensure we see the read progress
        const size_t read_idx = read_index_.value.load(std::memory_order_acquire);

        // Check full condition
        // One slot intentionally unused to disambiguate.
        // Effective capacity is Capacity - 1.
        if (static_cast<size_t>(write_idx - read_idx) >= (kCapacity - 1)) {
            return false;
        }

        // Construct element
        size_t offset = write_idx & kMask;
        new (GetSlot(offset)) T(element);

        // Publish
        /*
            memory_order_release: release ordering constraint
            why is it used here because producer thread is writing data so it needs to see the write progress

            so when someone tries to load write_idx it will see the new updated value if they are using memory_order_acquire likw we did above.
            memory_order means how memory is ordered around atomic operations for both atomic and non-atomic memory access to optimize performance.

            let' say one thread write following code
            x=1
            y=2
            flag.store(1,memory_release)
            z=3

            and anothre thread read following code
            flag.load(memory_acquire)
            x1=x
            y1=y
            z1=z

            then it is guaranteed that x1 will be 1,y1 will be 2, but z1 may/maynot be 3.
        */
        write_index_.value.store(write_idx + 1, std::memory_order_release);
        return true;
    }

    /**
     * @brief Attempts to push an element into the buffer (Move).
     *
     * @param element The element to move.
     * @return true if successful, false if full.
     */
    bool TryPush(T &&element) noexcept {
        const size_t write_idx = write_index_.value.load(std::memory_order_relaxed);
        const size_t read_idx = read_index_.value.load(std::memory_order_acquire);

        if (static_cast<size_t>(write_idx - read_idx) >= (kCapacity - 1)) {
            return false;
        }

        size_t offset = write_idx & kMask;
        new (GetSlot(offset)) T(std::move(element));

        write_index_.value.store(write_idx + 1, std::memory_order_release);
        return true;
    }

    /*Consumer Operations*/

    /**
     * @brief Attempts to pop an element from the buffer.
     *
     * @param out_element Destination for the popped element.
     * @return true if successful, false if empty.
     */
    bool TryPop(T &out_element) noexcept {
        // Load indices
        // Acquire load for producer's index to ensure we see the data
        /*
            see here we are using acquire for writting because the thread which is consuming data is the same
            thread which is responsible for writting read_idx so no need of synchronization.
        */
        const size_t write_idx = write_index_.value.load(std::memory_order_acquire);
        // Relaxed load for consumer's own index
        const size_t read_idx = read_index_.value.load(std::memory_order_relaxed);

        // Check empty condition
        if (read_idx == write_idx) {
            return false;
        }

        // Access element
        size_t offset = read_idx & kMask;
        T *slot = GetSlot(offset);

        // Move to output
        out_element = std::move(*slot);

        // Explicit destruction of the slot
        slot->~T();

        // Publish
        // Release semantics to signal that the slot is free
        read_index_.value.store(read_idx + 1, std::memory_order_release);
        return true;
    }

    /*Optional Observability (Non-Hot Path)*/

    /**
     * @return Approximate number of elements in the buffer.
     * Note: Result may be stale.
     */
    size_t Size() const noexcept {
        size_t w = write_index_.value.load(std::memory_order_relaxed);
        size_t r = read_index_.value.load(std::memory_order_relaxed);
        return w - r;
    }

    /**
     * @return true if the buffer is approximately empty.
     */
    bool Empty() const noexcept {
        return Size() == 0;
    }

    /**
     * @return true if the buffer is approximately full.
     */
    bool Full() const noexcept {
        return Size() >= (kCapacity - 1);
    }

  private:
    // Helper to get raw pointer to slot
    T *GetSlot(size_t index) noexcept {
        /*
            what reinterpret_cast does is it tells compiler that treat this address as T*.
            initially storage_ is unsigned char array so we are just accessing the address of that array
            and then we are telling compiler that treat this address as T*.


            everywhere GetSlot is passing with index so here it will return lets say 4*size(T)th address.
            knowing that next object's address is 5*size(T)th address so we have enough space to store obj.
        */
        return reinterpret_cast<T *>(&storage_[index * sizeof(T)]);
    }

    /*Storage and Alignment Layout*/

    // Structure to enforce cache line separation for atomic indices

    /*
    if we write alignas(64) IndexWrapper idx; it will be at address of multiple of 64 but it won't be padded to 64 bytes.
    but if we write
    struct alignof(64) Indexwrapper{
        int x; //only 4 byte;
    }
    then each object will be padded to 64 bytes.
    */
    struct alignas(kCacheLineSize) IndexWrapper {
        std::atomic<size_t> value{0};
    };

    // Producer-owned state
    IndexWrapper write_index_;

    // Consumer-owned state
    IndexWrapper read_index_;

    // Storage Buffer
    // Aligned to cache line to prevent false sharing with indices.
    // Also aligned to T to satisfy element requirements.
    alignas(kCacheLineSize) alignas(T) unsigned char storage_[Capacity * sizeof(T)];
};

} // namespace internal
} // namespace logger

#endif // LOGGER_INTERNAL_RING_BUFFER_H
