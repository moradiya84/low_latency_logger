#include "internal/ring_buffer.h"
#include <iostream>

struct Simple {
    int x;
};

struct NonTrivial {
    int *ptr;
    NonTrivial(int v) : ptr(new int(v)) {}
    ~NonTrivial() { delete ptr; }
    NonTrivial(const NonTrivial &other) : ptr(new int(*other.ptr)) {}
    NonTrivial(NonTrivial &&other) noexcept : ptr(other.ptr) { other.ptr = nullptr; }
    NonTrivial &operator=(NonTrivial &&other) noexcept {
        if (this != &other) {
            delete ptr;
            ptr = other.ptr;
            other.ptr = nullptr;
        }
        return *this;
    }
};

int main() {
    // Test 1: Simple type
    logger::internal::SpscRingBuffer<Simple, 16> buf1;
    Simple s{123};
    if (!buf1.TryPush(s))
        return 1;
    Simple out;
    if (!buf1.TryPop(out))
        return 2;
    if (out.x != 123)
        return 3;

    // Test 2: Move-only flow (simulated with smart move)
    logger::internal::SpscRingBuffer<NonTrivial, 8> buf2;
    buf2.TryPush(NonTrivial(42));
    NonTrivial nt(0);
    buf2.TryPop(nt);

    return 0;
}
