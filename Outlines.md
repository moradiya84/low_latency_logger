# Project: Low-Latency Asynchronous Logger (C++)

## Objective
- Build a high-performance, non-blocking logging system
- Minimize and bound logging latency on application threads
- Decouple log generation from disk I/O

## Core Characteristics
- Asynchronous design
- Single Producer → Single Consumer (SPSC)
- Fixed memory footprint
- Deterministic latency
- Binary log format

## Functional Requirements
- Application threads push log records
- Dedicated background thread writes logs to disk
- No disk I/O on producer threads
- Graceful shutdown with log draining

## Performance Constraints
- No heap allocations in the log hot path
- No mutexes in the log hot path
- Bounded queue capacity
- Predictable p99 latency
- Batched disk writes only

## Data Structures
- Preallocated SPSC ring buffer
- Power-of-two capacity
- Cache-line aligned atomic indices
- Fixed-size log record storage

## Log Record Format
- Timestamp (nanoseconds)
- Log level (enum or integer)
- Message length
- Fixed-size message buffer (truncate if oversized)

## Concurrency Model
- Producer threads write to ring buffer
- Consumer thread reads from ring buffer
- Lock-free communication using atomics
- Acquire/release memory ordering semantics

## Backpressure Strategy
- Explicit policy required:
  - Drop new logs when buffer is full (default)
  - OR overwrite oldest log entries

## Timestamping
- Monotonic clock source
- No dependency on wall-clock time
- Low-overhead timestamp capture

## I/O Strategy
- Sequential file writes
- Large batched writes (KB-scale)
- Buffered I/O
- Periodic or size-based flushing

## Error Handling
- Logging failures must not crash the application
- Producer returns status on log attempt
- I/O errors handled only by consumer thread

## Configuration
- Ring buffer size (compile-time or init-time)
- Log file path
- Log level threshold
- Flush interval or batch size

## Benchmarking & Validation
- Measure producer latency (p50, p99)
- Measure throughput (logs/sec)
- Verify zero allocations after initialization
- Validate correctness under sustained load

## Out of Scope
- Network logging
- Log rotation
- Text formatting
- Distributed logging
- Encryption (future extension only)

## Deliverables
- C++20 source code
- Benchmark executable
- Latency and throughput metrics
- README documenting design decisions

