#include "../internal/ring_buffer.h"

#include <cassert>
#include <cstddef>

int main() {

    // Use power-of-two capacity; effective capacity is Capacity - 1.
    constexpr std::size_t kCapacity = 8;
    constexpr std::size_t kEffectiveCapacity = kCapacity - 1;

    logger::internal::SpscRingBuffer<int, kCapacity> buffer;

    // Buffer starts empty.
    assert(buffer.Empty());
    assert(buffer.Size() == 0);

    // Fill to effective capacity.
    for (int i = 0; i < static_cast<int>(kEffectiveCapacity); ++i) {
        assert(buffer.TryPush(i));
    }
    assert(buffer.Full());
    assert(buffer.Size() == kEffectiveCapacity);

    // One more push should fail when full.
    assert(!buffer.TryPush(999));

    // Pop in FIFO order.
    for (int i = 0; i < static_cast<int>(kEffectiveCapacity); ++i) {
        int value = -1;
        assert(buffer.TryPop(value));
        assert(value == i);
    }
    assert(buffer.Empty());

    // Wrap-around behavior: push, pop a few, push more, then drain.
    for (int i = 0; i < static_cast<int>(kEffectiveCapacity); ++i) {
        assert(buffer.TryPush(100 + i));
    }

    for (int i = 0; i < 3; ++i) {
        int value = -1;
        assert(buffer.TryPop(value));
        assert(value == 100 + i);
    }

    for (int i = 0; i < 3; ++i) {
        assert(buffer.TryPush(200 + i));
    }

    // Remaining expected order after wrap: 103..106 then 200..202.
    const int expected[] = {103, 104, 105, 106, 200, 201, 202};
    for (int expected_value : expected) {
        int value = -1;
        assert(buffer.TryPop(value));
        assert(value == expected_value);
    }

    assert(buffer.Empty());
    return 0;
}
