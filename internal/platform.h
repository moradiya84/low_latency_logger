/**
 * @file platform.h
 * @brief Platform abstraction layer for low-latency logging
 *
 * Provides portable macros and utilities for:
 * - Compiler detection
 * - Operating system detection
 * - Branch prediction hints
 * - Force-inline / no-inline macros
 * - CPU pause / relax instructions
 * - High-resolution time access
 * - Memory prefetch hints
 * - Thread-local storage
 * - Debug utilities
 */

#ifndef LOGGER_INTERNAL_PLATFORM_H
#define LOGGER_INTERNAL_PLATFORM_H

#include <cstdint>

/* COMPILER DETECTION */

/**
 * Compiler identification macros.
 * Note: Order matters - Clang defines __GNUC__, so check Clang first.
 */
#if defined(__clang__)
#define LOGGER_COMPILER_CLANG 1
#define LOGGER_COMPILER_NAME "Clang"
#define LOGGER_COMPILER_VERSION (__clang_major__ * 10000 + __clang_minor__ * 100 + __clang_patchlevel__)
#elif defined(__GNUC__)
#define LOGGER_COMPILER_GCC 1
#define LOGGER_COMPILER_NAME "GCC"
#define LOGGER_COMPILER_VERSION (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)
#else
#define LOGGER_COMPILER_UNKNOWN 1
#define LOGGER_COMPILER_NAME "Unknown"
#define LOGGER_COMPILER_VERSION 0
#endif

/* Convenience check for GCC-compatible compilers (GCC or Clang) */
#if defined(LOGGER_COMPILER_GCC) || defined(LOGGER_COMPILER_CLANG)
#define LOGGER_COMPILER_GCC_COMPATIBLE 1
#endif

/* OPERATING SYSTEM DETECTION */
/*
    posix is is kind of switch to know whether or not we are going to use posix backend for
    timestamping , thread-local storage, safe flushing and shutdown
*/
#if defined(__APPLE__) && defined(__MACH__)
#define LOGGER_OS_MACOS 1
#define LOGGER_OS_NAME "macOS"
#define LOGGER_OS_POSIX 1
#elif defined(__linux__)
#define LOGGER_OS_LINUX 1
#define LOGGER_OS_NAME "Linux"
#define LOGGER_OS_POSIX 1
#elif defined(_WIN32) || defined(_WIN64)
#define LOGGER_OS_WINDOWS 1
#define LOGGER_OS_NAME "Windows"
#elif defined(__FreeBSD__)
#define LOGGER_OS_FREEBSD 1
#define LOGGER_OS_NAME "FreeBSD"
#define LOGGER_OS_POSIX 1
#elif defined(__unix__)
#define LOGGER_OS_UNIX 1
#define LOGGER_OS_NAME "Unix"
#define LOGGER_OS_POSIX 1
#else
#define LOGGER_OS_UNKNOWN 1
#define LOGGER_OS_NAME "Unknown"
#endif

/* ARCHITECTURE DETECTION */
#if defined(__aarch64__) || defined(_M_ARM64)
#define LOGGER_ARCH_ARM64 1
#define LOGGER_ARCH_NAME "ARM64"
#elif defined(__x86_64__) || defined(_M_X64)
#define LOGGER_ARCH_X64 1
#define LOGGER_ARCH_NAME "x86_64"
#elif defined(__i386__) || defined(_M_IX86)
#define LOGGER_ARCH_X86 1
#define LOGGER_ARCH_NAME "x86"
#elif defined(__arm__) || defined(_M_ARM)
#define LOGGER_ARCH_ARM 1
#define LOGGER_ARCH_NAME "ARM"
#else
#define LOGGER_ARCH_UNKNOWN 1
#define LOGGER_ARCH_NAME "Unknown"
#endif

// Apple Silicon specific detection
#if defined(__APPLE__) && defined(__aarch64__)
#define LOGGER_ARCH_APPLE_SILICON 1
#endif

/* BRANCH PREDICTION HINTS */

/**
 * @def LOGGER_LIKELY(x)
 * @brief Hint to compiler that condition is likely to be true
 *
 * Usage:
 *   if (LOGGER_LIKELY(success)) { ... }
 */

/**
 * @def LOGGER_UNLIKELY(x)
 * @brief Hint to compiler that condition is unlikely to be true
 *
 * Usage:
 *   if (LOGGER_UNLIKELY(error)) { handle_error(); }
 */
#if defined(LOGGER_COMPILER_GCC_COMPATIBLE)
/* __builtin_expect is a GCC/Clang extension that tells the compiler
    whether a condition is likely to be true or false.
    It is used to optimize the generated code.
    based on this compiler decide which part of code deserve faster cache loading so it can improve performance
    !!(x) is used to convert x to boolean
*/
#define LOGGER_LIKELY(x) __builtin_expect(!!(x), 1)
#define LOGGER_UNLIKELY(x) __builtin_expect(!!(x), 0)
#elif defined(LOGGER_COMPILER_MSVC) && (_MSC_VER >= 1926)
// MSVC 2019 16.6+ supports [[likely]] and [[unlikely]]
// But for condition hints, we need different approach
#define LOGGER_LIKELY(x) (x)
#define LOGGER_UNLIKELY(x) (x)
#else
#define LOGGER_LIKELY(x) (x)
#define LOGGER_UNLIKELY(x) (x)
#endif

// C++20 attribute-style branch hints (for if/else/switch statements)
/*
    [[likely]] and [[unlikely]] are C++20 attributes that tell the compiler
    whether a condition is likely to be true or false.
    unlike builtin_expect these are not compiler specific and can be used in any C++20 compliant compiler
    but they are not as powerful as builtin_expect
*/
#if __cplusplus >= 202002L
#define LOGGER_ATTR_LIKELY [[likely]]
#define LOGGER_ATTR_UNLIKELY [[unlikely]]
#else
#define LOGGER_ATTR_LIKELY
#define LOGGER_ATTR_UNLIKELY
#endif

/*FORCE INLINE AND NO INLINE MACROS */

/*
 * @def LOGGER_FORCE_INLINE
 * @brief Force function to be inlined (hot path optimization)
 *
 * Usage:
 *   LOGGER_FORCE_INLINE void HotFunction() { ... }
 */
#if defined(LOGGER_COMPILER_GCC_COMPATIBLE)
#define LOGGER_FORCE_INLINE inline __attribute__((always_inline))
#elif defined(LOGGER_COMPILER_MSVC)
#define LOGGER_FORCE_INLINE __forceinline
#else
#define LOGGER_FORCE_INLINE inline
#endif

/**
 * @def LOGGER_NO_INLINE
 * @brief Prevent function from being inlined (cold path, error handling)
 *
 * Usage:
 *   LOGGER_NO_INLINE void HandleError() { ... }
 */
#if defined(LOGGER_COMPILER_GCC_COMPATIBLE)
#define LOGGER_NO_INLINE __attribute__((noinline))
#elif defined(LOGGER_COMPILER_MSVC)
#define LOGGER_NO_INLINE __declspec(noinline)
#else
#define LOGGER_NO_INLINE
#endif

/*
    Hot and cold is based on how often function is called
    hot function are called frequently so compiler optimize for speed
    cold function are called rarely so compiler optimize for size meaning it will reduce code size
*/
/**
 * @def LOGGER_COLD
 * @brief Hint that function is rarely called (optimizes for size over speed)
 */
#if defined(LOGGER_COMPILER_GCC_COMPATIBLE)
#define LOGGER_COLD __attribute__((cold))
#else
#define LOGGER_COLD
#endif

/**
 * @def LOGGER_HOT
 * @brief Hint that function is frequently called (optimizes for speed)
 */
#if defined(LOGGER_COMPILER_GCC_COMPATIBLE)
#define LOGGER_HOT __attribute__((hot))
#else
#define LOGGER_HOT
#endif

/* CPU PAUSE / RELAX INSTRUCTIONS */

/**
 * @def LOGGER_CPU_RELAX()
 * @brief CPU pause/relax hint for spin-wait loops
 *
 * Reduces power consumption and pipeline stalls in spin loops.
 * Essential for efficient busy-waiting in lock-free algorithms.
 *
 * Usage:
    so like in atomic indec while one thread is waiting for other thread to write we can relax cpu to reduce power consumption
 *   while (!flag.load(std::memory_order_acquire)) {
 *       LOGGER_CPU_RELAX();
 *   }
 */
#if defined(LOGGER_COMPILER_GCC_COMPATIBLE)
#if defined(LOGGER_ARCH_X64) || defined(LOGGER_ARCH_X86)
// x86 PAUSE instruction
#define LOGGER_CPU_RELAX() __asm__ __volatile__("pause" ::: "memory")
#elif defined(LOGGER_ARCH_ARM64)
// ARM64 YIELD instruction
#define LOGGER_CPU_RELAX() __asm__ __volatile__("yield" ::: "memory")
#elif defined(LOGGER_ARCH_ARM)
// ARM YIELD instruction
#define LOGGER_CPU_RELAX() __asm__ __volatile__("yield" ::: "memory")
#else
// Generic: compiler barrier only
#define LOGGER_CPU_RELAX() __asm__ __volatile__("" ::: "memory")
#endif
#elif defined(LOGGER_COMPILER_MSVC)
#if defined(LOGGER_ARCH_X64) || defined(LOGGER_ARCH_X86)
#include <intrin.h>
#define LOGGER_CPU_RELAX() _mm_pause()
#elif defined(LOGGER_ARCH_ARM64)
#include <intrin.h>
#define LOGGER_CPU_RELAX() __yield()
#else
#define LOGGER_CPU_RELAX() ((void)0)
#endif
#else
#define LOGGER_CPU_RELAX() ((void)0)
#endif

/* HIGH-RESOLUTION TIME / CLOCK ACCESS */

namespace logger {
namespace internal {

/**
 * @brief Read CPU timestamp counter (TSC) - lowest overhead timing
 *
 * WARNING: TSC may not be synchronized across cores on some systems.
 * Prefer std::chrono::steady_clock for portable wall-clock time.
 * Use this only when nanosecond-level overhead matters.
 * time stamp might not be accurate. but it will be fast
   it might not be synchronized across cores on some systems hence without serializing instructions it might not be accurate
 * @return CPU timestamp counter value
 */
LOGGER_FORCE_INLINE std::uint64_t ReadTsc() noexcept {
#if defined(LOGGER_COMPILER_GCC_COMPATIBLE)
#if defined(LOGGER_ARCH_ARM64)
    std::uint64_t val;
    __asm__ __volatile__("mrs %0, cntvct_el0" : "=r"(val));
    return val;
#elif defined(LOGGER_ARCH_X64) || defined(LOGGER_ARCH_X86)
    std::uint32_t lo, hi;
    __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
    return (static_cast<std::uint64_t>(hi) << 32) | lo;
#else
    // Fallback: no direct TSC access
    return 0;
#endif
#elif defined(LOGGER_COMPILER_MSVC)
#if defined(LOGGER_ARCH_X64) || defined(LOGGER_ARCH_X86)
    return __rdtsc();
#elif defined(LOGGER_ARCH_ARM64)
    return _ReadStatusReg(ARM64_CNTVCT);
#else
    return 0;
#endif
#else
    return 0;
#endif
}

/**
 * @brief Read TSC with serialization (RDTSCP on x86)
 *
 * Provides stronger ordering guarantees than ReadTsc().
 * Also returns the processor ID (on x86).
 * it does not allow compiler to serialize instructions and give accurate time stamp counter
 * @param[out] processor_id Optional pointer to store processor ID
 * @return CPU timestamp counter value
 */
/*
    why pass processor id because if we are on x85 or x64 we can get processor id from rdtscp instruction
    and in case it got changed because of thread migration we can get it from aux register
    and we know why spike in latency happen.

    we are not doing in arm because it takes extra call to get processor id which is not required at this point
*/
LOGGER_FORCE_INLINE std::uint64_t ReadTscp(std::uint32_t *processor_id = nullptr) noexcept {
#if defined(LOGGER_COMPILER_GCC_COMPATIBLE) && (defined(LOGGER_ARCH_X64) || defined(LOGGER_ARCH_X86))
    // x86/x64: Use RDTSCP which provides serialization and processor ID

    std::uint32_t lo, hi, aux;
    __asm__ __volatile__("rdtscp" : "=a"(lo), "=d"(hi), "=c"(aux));
    if (processor_id) {
        *processor_id = aux;
    }
    return (static_cast<std::uint64_t>(hi) << 32) | lo;

#elif defined(LOGGER_COMPILER_GCC_COMPATIBLE) && defined(LOGGER_ARCH_ARM64)
    // ARM64: Use ISB (Instruction Synchronization Barrier) for serialization
    // then read the virtual counter
    std::uint64_t val;
    // ISB ensures all previous instructions complete before reading counter
    __asm__ __volatile__("isb" ::: "memory");               // make sure all previous instructions are completed before reading counter
    __asm__ __volatile__("mrs %0, cntvct_el0" : "=r"(val)); // read the virtual counter

    return val;

#elif defined(LOGGER_COMPILER_MSVC) && (defined(LOGGER_ARCH_X64) || defined(LOGGER_ARCH_X86))
    unsigned int aux;
    std::uint64_t tsc = __rdtscp(&aux);
    if (processor_id) {
        *processor_id = aux;
    }
    return tsc;
#else
    // Fallback for unknown architectures
    if (processor_id) {
        *processor_id = 0;
    }
    return ReadTsc();
#endif
}

} // namespace internal
} // namespace logger

/* PREFETCH HINTS */

/**
 * @def LOGGER_PREFETCH_READ(addr)
 * @brief Prefetch memory for reading
 */

/**sp
 * @def LOGGER_PREFETCH_WRITE(addr)
 * @brief Prefetch memory for writing

    __builtin_prefetch(address,read/write,locality)
    read/write: 0 for read, 1 for write
    locality: 0 for non-temporal, 1 for L3, 2 for L2, 3 for L1 (3 means it says longer in cache);
    we use this to prefetch data in cache so that it can be used by cpu faster
 */
#if defined(LOGGER_COMPILER_GCC_COMPATIBLE)
#define LOGGER_PREFETCH_READ(addr) __builtin_prefetch((addr), 0, 3)
#define LOGGER_PREFETCH_WRITE(addr) __builtin_prefetch((addr), 1, 3)
#elif defined(LOGGER_COMPILER_MSVC) && (defined(LOGGER_ARCH_X64) || defined(LOGGER_ARCH_X86))
#include <xmmintrin.h>
#define LOGGER_PREFETCH_READ(addr) _mm_prefetch(static_cast<const char *>(addr), _MM_HINT_T0)
#define LOGGER_PREFETCH_WRITE(addr) _mm_prefetch(static_cast<const char *>(addr), _MM_HINT_T0)
#else
#define LOGGER_PREFETCH_READ(addr) ((void)(addr))
#define LOGGER_PREFETCH_WRITE(addr) ((void)(addr))
#endif

/* UNREACHABLE HINTS */

/**
 * @def LOGGER_UNREACHABLE()
 * @brief Indicates code path is unreachable (undefined behavior if reached)
 *
 * Helps compiler optimize by eliminating dead code paths.
 *
 * Usage:
 *   switch (type) {
 *       case A: ...
 *       default: LOGGER_UNREACHABLE();
 *   }
    so compiler know that there is only 1 option so this is hot path and it can optimize it and remove dead code
    if it reaches that point it's undefined behavior may/maynot be crash
 */
#if defined(LOGGER_COMPILER_GCC_COMPATIBLE)
#define LOGGER_UNREACHABLE() __builtin_unreachable()
#elif defined(LOGGER_COMPILER_MSVC)
#define LOGGER_UNREACHABLE() __assume(0)
#else
#define LOGGER_UNREACHABLE() ((void)0)
#endif

/* ASSUME HINTS */

/**
 * @def LOGGER_ASSUME(cond)
 * @brief Tell compiler to assume condition is true
 *
 * Allows compiler to optimize based on the assumption.
 * based on this info compiler can remove dead code
 * WARNING: Undefined behavior if condition is actually false!
 */
#if defined(LOGGER_COMPILER_GCC_COMPATIBLE) && __GNUC__ >= 13
#define LOGGER_ASSUME(cond) __attribute__((assume(cond)))
#elif defined(LOGGER_COMPILER_CLANG)
#define LOGGER_ASSUME(cond) __builtin_assume(cond)
#elif defined(LOGGER_COMPILER_MSVC)
#define LOGGER_ASSUME(cond) __assume(cond)
#else
#define LOGGER_ASSUME(cond) ((void)0)
#endif

/* THREAD-LOCAL STORAGE */

/**
 * @def LOGGER_THREAD_LOCAL
 * @brief Thread-local storage specifier
 *
 * Usage:
 *   LOGGER_THREAD_LOCAL int thread_id;
 *   thread_local create variable in each thread and they don't share memory this is trade off
 *   so let's say we have thread_local int x=0;
 *   and two different thread call function increment(){ x++;} then this 2 thread have different copy of x both with value of 1
 *   if we write cout<<x<<endl; in main function we will get 0;

    benefit? it remove race condition by just reomoving all the syncronization
 */
#if defined(LOGGER_COMPILER_GCC_COMPATIBLE) || defined(LOGGER_COMPILER_MSVC)
#define LOGGER_THREAD_LOCAL thread_local
#else
#define LOGGER_THREAD_LOCAL
#endif

/* DEBUG UTILITIES */

/**
 * @def LOGGER_DEBUG_BREAK()
 * @brief Trigger a debugger breakpoint
    if compiler read this it will stop execution and wait for debugger to attach
    if debugger is not attached it will crash
 */
#if defined(LOGGER_COMPILER_GCC_COMPATIBLE)
#if defined(LOGGER_ARCH_X64) || defined(LOGGER_ARCH_X86)
#define LOGGER_DEBUG_BREAK() __asm__ __volatile__("int3")
#elif defined(LOGGER_ARCH_ARM64) || defined(LOGGER_ARCH_ARM)
#define LOGGER_DEBUG_BREAK() __builtin_trap()
#else
#define LOGGER_DEBUG_BREAK() __builtin_trap()
#endif
#elif defined(LOGGER_COMPILER_MSVC)
#define LOGGER_DEBUG_BREAK() __debugbreak()
#else
#define LOGGER_DEBUG_BREAK() ((void)0)
#endif

/* RESTRICT POINTER HINT */

/**
 * @def LOGGER_RESTRICT
 * @brief Restrict pointer hint (no aliasing)
    Compilers are conservative. If you pass multiple pointers to a function,
    the compiler must assume they might point to overlapping memory. That assumption blocks many optimizations.
    __restrict__ removes that assumption.
 */
#if defined(LOGGER_COMPILER_GCC_COMPATIBLE)
#define LOGGER_RESTRICT __restrict__
#elif defined(LOGGER_COMPILER_MSVC)
#define LOGGER_RESTRICT __restrict
#else
#define LOGGER_RESTRICT
#endif

/* NODISCARD / DEPRECATED ATTRIBUTES */

/**
 * @def LOGGER_NODISCARD
 * @brief Warn if return value is discarded
    if function return something and we don't use it compiler will give warning
    this is useful for function that return status or error code
 */
#if __cplusplus >= 201703L
#define LOGGER_NODISCARD [[nodiscard]]
#elif defined(LOGGER_COMPILER_GCC_COMPATIBLE)
#define LOGGER_NODISCARD __attribute__((warn_unused_result))
#elif defined(LOGGER_COMPILER_MSVC)
#define LOGGER_NODISCARD _Check_return_
#else
#define LOGGER_NODISCARD
#endif

/**
 * @def LOGGER_DEPRECATED(msg)
 * @brief Mark function as deprecated with message

   to warn developers at compile time when they rely on something you intend to remove or replace.
   it gives warning at compile time
 */
#if __cplusplus >= 201402L
#define LOGGER_DEPRECATED(msg) [[deprecated(msg)]]
#elif defined(LOGGER_COMPILER_GCC_COMPATIBLE)
#define LOGGER_DEPRECATED(msg) __attribute__((deprecated(msg)))
#elif defined(LOGGER_COMPILER_MSVC)
#define LOGGER_DEPRECATED(msg) __declspec(deprecated(msg))
#else
#define LOGGER_DEPRECATED(msg)
#endif

/* VISIBILITY / EXPORT MACROS */

/**
 * @def LOGGER_EXPORT
 * @brief Mark symbol for export from shared library
    __visibility("default") is used to mark a function or variable as public,
    meaning it will be visible outside the shared library.
 */

/**
 * @def LOGGER_HIDDEN
 * @brief Hide symbol from shared library export
    __visibility("hidden") is used to mark a function or variable as private,
    meaning it will not be visible outside the shared library.
 */
#if defined(LOGGER_COMPILER_GCC_COMPATIBLE)
#define LOGGER_EXPORT __attribute__((visibility("default")))
#define LOGGER_HIDDEN __attribute__((visibility("hidden")))
#elif defined(LOGGER_COMPILER_MSVC)
#define LOGGER_EXPORT __declspec(dllexport)
#define LOGGER_HIDDEN
#else
#define LOGGER_EXPORT
#define LOGGER_HIDDEN
#endif

/* COMPILE-TIME SANITY CHECKS */

// Helper to verify we detected at least one compiler
#if !defined(LOGGER_COMPILER_CLANG) && !defined(LOGGER_COMPILER_GCC) && !defined(LOGGER_COMPILER_MSVC) && !defined(LOGGER_COMPILER_INTEL) && !defined(LOGGER_COMPILER_UNKNOWN)
#error "Compiler detection failed: no compiler detected"
#endif

#endif // LOGGER_INTERNAL_PLATFORM_H
