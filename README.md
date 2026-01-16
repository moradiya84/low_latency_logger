Assumptions:
-  This project is built considering user is using gcc or clang compiler
-  This project is built considering user is using posix or windows os

# Low Latency Logger Internals

A header-only C++ library for high-performance, lock-free logging, designed to minimize latency spikes through cache-aware data structures and atomic operations.

## Why It Exists
To eliminate locking overhead and non-deterministic latency. It uses lock-free synchronization, explicit cache line padding to prevent false sharing, and hardware-specific optimizations (M4/x86).

## Components (`internal/`)
*   **`ring_buffer.h`**: A lock-free Single-Producer/Single-Consumer (SPSC) ring buffer. Uses atomic head/tail indices with acquire/release semantics and prevents false sharing.
*   **`cacheline.h`**: Alignment utilities. Defines `kCacheLineSize` (128B for Apple Silicon, 64B otherwise) and padding helpers like `CachelinePad` to optimize memory layout.
*   **`platform.h`**: Abstraction layer for compiler, OS, and architecture detection. Provides performance macros like `LOGGER_LIKELY`, `LOGGER_FORCE_INLINE`, and `ReadTsc()`.

## How to Work With It
Include the headers in your C++17+ project:

```cpp
#include "internal/ring_buffer.h"
#include "internal/platform.h"

// Instantiate with power-of-2 capacity
logger::internal::SpscRingBuffer<MyLogEntry, 1024> buffer;

// Producer
if (buffer.TryPush(entry)) { ... }

// Consumer
MyLogEntry item;
if (buffer.TryPop(item)) { ... }
```

## How to Run
This is a header-only library. Compile your application with a C++17 compliant compiler (GCC/Clang) using architecture flags (e.g., `-march=native`).