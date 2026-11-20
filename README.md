# Low-Latency Asynchronous Logger

A high-performance, lock-free logging library in C++ designed for latency-sensitive systems where **every microsecond counts**.

---

## The Core Innovation

> **Zero heap allocations. Zero mutexes. Zero blocking on the hot path.**

This is not just a feature—it's the **fundamental design principle** that drives every architectural decision in this logger. While most logging libraries introduce unpredictable latency through dynamic memory allocation and lock contention, this logger guarantees:

- **No `malloc`/`new`** during log calls
- **No mutex locks** that could cause thread blocking
- **Fixed, bounded latency** regardless of I/O conditions
---

## Key Features

| Feature | Description |
|---------|-------------|
| **Lock-Free Hot Path** | Producer threads never block—uses atomics with acquire/release semantics |
| **SPSC Ring Buffer** | Single-producer/single-consumer design eliminates contention |
| **Fixed Memory Footprint** | All buffers preallocated at initialization |
| **Cache-Line Aligned** | Data structures aligned to prevent false sharing |
| **TSC Timestamping** | Sub-nanosecond precision using CPU timestamp counter |
| **Compile-Time Config** | Feature toggles via preprocessor for zero-cost abstractions |

---

## Architecture

```
┌─────────────────┐     Lock-Free      ┌──────────────────┐
│  Producer       │    SPSC Queue      │   Consumer       │
│  (App Thread)   │ ───────────────►   │   (Background)   │
│                 │    Atomics Only    │                  │
│  • Log()        │                    │  • Drain buffer  │
│  • ~42 ns p50   │                    │  • Batch writes  │
└─────────────────┘                    └────────┬─────────┘
                                                │
                                                ▼
                                       ┌────────────────┐
                                       │   Sink (I/O)   │
                                       │   File/stdout  │
                                       └────────────────┘
```

### Design Decisions

- **Preallocated SPSC Ring Buffer**: Power-of-two capacity with cache-line separated atomic indices
- **Fixed-Size Log Records**: No dynamic allocation—messages truncated if over `LOGGER_MAX_MESSAGE_SIZE`
- **Backpressure Policy**: Drop new logs when buffer is full (configurable)
- **Batched I/O**: Consumer thread aggregates writes to minimize syscall overhead

---

## Project Structure

```
low_latency_logger/
├── include/           # Public API headers
│   ├── logger.h       # Main Logger class interface
│   ├── record.h       # Fixed-size LogRecord structure
│   ├── level.h        # Log levels (Trace → Fatal)
│   ├── sink.h         # Output sink abstraction
│   ├── formatter.h    # Log formatting
│   ├── consumer.h     # Background consumer thread
│   └── config.h       # Compile-time configuration
├── internal/          # Internal implementation
│   ├── ring_buffer.h  # Lock-free SPSC ring buffer
│   ├── cacheline.h    # Cache-line alignment utilities
│   ├── platform.h     # Platform detection & intrinsics
│   └── clock.h        # High-resolution timing
├── src/               # Implementation files
├── tests/             # Test suite
└── benchmarks/        # Performance benchmarks
```

---

## Build

**Requirements**: C++17 or later, CMake 3.16+

```sh
# Configure and build
cmake -S . -B build && cmake --build build

# Run tests
cd build && ctest --output-on-failure

# Build with benchmarks
cmake -S . -B build -DLLL_BUILD_BENCHMARKS=ON && cmake --build build
```

---

## Configuration

Compile-time options can be set via CMake or compiler flags:

| Macro | Default | Description |
|-------|---------|-------------|
| `LOGGER_MAX_MESSAGE_SIZE` | 1024 | Max message payload in bytes |
| `LOGGER_ENABLE_THREAD_ID` | 1 | Capture thread ID per log |
| `LOGGER_ENABLE_SOURCE_LOCATION` | 1 | Capture `__FILE__`, `__LINE__`, `__func__` |
| `LOGGER_BACKEND_SPIN_COUNT` | 1000 | Spin iterations before yielding |

```sh
cmake -S . -B build -DCMAKE_CXX_FLAGS="-DLOGGER_MAX_MESSAGE_SIZE=2048"
```

---

## Log Record Format

Each log record contains:

| Field | Size | Description |
|-------|------|-------------|
| `level` | 1 byte | Log severity (Trace, Debug, Info, Warn, Error, Fatal) |
| `timestamp` | 8 bytes | TSC-derived monotonic timestamp |
| `message_length` | 8 bytes | Actual message length |
| `thread_id` | 8 bytes | *(optional)* Calling thread ID |
| `file` | 8 bytes | *(optional)* Pointer to source file name |
| `function` | 8 bytes | *(optional)* Pointer to function name |
| `line` | 4 bytes | *(optional)* Source line number |
| `message` | N bytes | Fixed-size message buffer |

All fields are cache-line padded to prevent false sharing in the ring buffer.

---

## Performance Characteristics

Measured on [Apple M4 variant], [macOS version], [clang version], `-O3`, [date].
Producer-side `Log()` latency, paced at 500K msgs/s, 2M calls, 64-byte message,
zero drops.

| Logger | p50 | p99 | p99.9 |
|--------|-----|-----|-------|
| **This logger** | 42 ns | 83 ns | 375 ns |
| Synchronous `fprintf` (buffered) | 83 ns | 1,500 ns | 5,000 ns |
| Mutex + `std::queue<std::string>` async | 583 ns | 1,208 ns | 7,583 ns |

Batch-averaged (100 calls/sample): ~51 ns mean, p99 163 ns.

**Measurement notes**
- Apple silicon's clock ticks at 24 MHz, so single-call readings are quantised to
  ~41 ns steps (the measured timer floor is 41 ns). The batch-averaged row gives
  finer resolution but smooths the tail.
- Burst test (5M calls at max speed): 97% dropped by design (drop-on-full policy);
  all 142,810 accepted records reached the sink (no loss). Delivered rate: 6.9M msgs/s.
- Each `LogRecord` is 1792 bytes (fields are cache-line padded, 128 B lines on M4).

### Reproduce
```sh
clang++ -std=c++17 -O3 -DNDEBUG benchmarks/log_throughput.cpp \
  src/formatter.cpp src/sink.cpp src/clock.cpp src/encoder.cpp -o log_throughput
./log_throughput
```

---

## Out of Scope

The following are explicitly **not** goals of this project:

- Network/remote logging
- Log rotation or compression
- Rich text formatting (JSON, XML)
- Distributed tracing
- Encryption

---

## License

[Add your license here]
