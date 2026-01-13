/**
 * @file cacheline.h
 * @brief Cache line alignment utilities for low-latency logging
 *
 * Provides cache line size constants, alignment macros, and padding helpers
 * optimized for Apple M4 architecture.
 */

#ifndef LOGGER_INTERNAL_CACHELINE_H
#define LOGGER_INTERNAL_CACHELINE_H

#include <cstddef>
#include <new>
#include <type_traits>

namespace logger {
namespace internal {

// Architecture-specific cache line size overrides (compile-time)

#if defined(__APPLE__) && defined(__arm64__)
// Apple Silicon (M1/M2/M3/M4) uses 128-byte cache lines
inline constexpr std::size_t kCacheLineSize = 128;
#elif defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
// x86/x86_64 uses 64-byte cache lines
inline constexpr std::size_t kCacheLineSize = 64;
#elif defined(__aarch64__) || defined(_M_ARM64)
// Generic ARM64 (non-Apple) typically uses 64-byte cache lines
inline constexpr std::size_t kCacheLineSize = 64;
#else
// Conservative default
inline constexpr std::size_t kCacheLineSize = 64;
#endif

// Destructive and Constructive interference size constants

/**
 * @brief Minimum offset between two objects to avoid false sharing
 *
 * Use this when placing objects that are accessed by different threads
 * to prevent cache line ping-pong.
 */

/*
  __cpp_lib_hardware_interference_size is a feature test macro that is defined
  in C++17 and later. It is used to check if the hardware_destructive_interference_size
  and hardware_constructive_interference_size are available(defined or not).


  201703L is the value of the feature test macro for C++17.(like when was it added)

  constexpr means that the value of the variable can be computed or known at compile time.
  constexpr variables always should be known at compile time.
  constexpr functions CAN be called at compile time.

  destructive_interference_size is the minimum offset between two objects to avoid false sharing.
*/
#if defined(__cpp_lib_hardware_interference_size) && __cpp_lib_hardware_interference_size >= 201703L
inline constexpr std::size_t kDestructiveInterferenceSize = (std::hardware_destructive_interference_size > kCacheLineSize) ? std::hardware_destructive_interference_size : kCacheLineSize;
#else
inline constexpr std::size_t kDestructiveInterferenceSize = (std::hardware_destructive_interference_size > kCacheLineSize) ? std::hardware_destructive_interference_size : kCacheLineSize;
#endif

/**
 * @brief Maximum size of contiguous memory to promote true sharing
 *
 * Use this when placing objects that are accessed together by the same thread
 * to maximize cache utilization.
 */

/*
  constructive_interference_size is the maximum size of contiguous memory to promote true sharing(in case when it is beneficial to have objects in the same cache line).
*/
#if defined(__cpp_lib_hardware_interference_size) && __cpp_lib_hardware_interference_size >= 201703L
inline constexpr std::size_t kConstructiveInterferenceSize = (std::hardware_constructive_interference_size < kCacheLineSize) ? std::hardware_constructive_interference_size : kCacheLineSize;
#else
inline constexpr std::size_t kConstructiveInterferenceSize = (std::hardware_constructive_interference_size < kCacheLineSize) ? std::hardware_constructive_interference_size : kCacheLineSize;
#endif

// Cache-line alignment macro / attribute

/**
 * @def CACHELINE_ALIGNED
 * @brief Aligns a variable or struct to cache line boundary
 *
 * Usage:
 *   struct CACHELINE_ALIGNED MyStruct { ... };
 *   CACHELINE_ALIGNED int counter;

 * alignas() is a C++ keyword that is used to specify the alignment of a variable or struct.
 * if we write alignas(32) each variable or object's adress will start at multiple of 32.
 */
#if defined(__GNUC__) || defined(__clang__)
#define CACHELINE_ALIGNED alignas(::logger::internal::kCacheLineSize)
#else
#define CACHELINE_ALIGNED alignas(::logger::internal::kCacheLineSize)
#endif

// Cache-line padding helpers

/**
 * @def CACHELINE_PAD
 * @brief Creates padding bytes to fill remainder of cache line
 * @param n Unique identifier for the padding (to avoid name collisions)
 *
 * Usage:
 *   struct MyStruct {
 *       int value;
 *       CACHELINE_PAD(0);  // Pads to next cache line boundary
 *   };

    we are creating array of 1 cacheline size.
    ##n is to create unique name for each array it create cacheline_padding_1/2/3
 */
#define CACHELINE_PAD(n) char cacheline_padding_##n[::logger::internal::kCacheLineSize]

/**
 * @def CACHELINE_PAD_AFTER
 * @brief Creates padding to fill cache line after a specific size
 * @param used_bytes Number of bytes already used in the cache line
 * @param n Unique identifier for the padding
 *
 * Usage:
 *   struct MyStruct {
 *       int value;  // 4 bytes
 *       CACHELINE_PAD_AFTER(sizeof(int), 0);  // Pads remaining bytes
 *   };
 */
#define CACHELINE_PAD_AFTER(used_bytes, n) char cacheline_padding_##n[(::logger::internal::kCacheLineSize - ((used_bytes) % ::logger::internal::kCacheLineSize)) % ::logger::internal::kCacheLineSize]

/**
 * @brief Struct template for cache line padding
 * @tparam N Size in bytes already used, padding fills the rest
 * @tparam Enable SFINAE(substitution failure is not an error) helper (do not specify)
 *
 * Usage:
 *   struct MyStruct {
 *       int value;
 *       CachelinePad<sizeof(int)> pad;
 *   };
 */
template <std::size_t N, typename Enable = void>
struct CachelinePad {
    static constexpr std::size_t kPaddingSize = (kCacheLineSize - (N % kCacheLineSize)) % kCacheLineSize;
    char padding[kPaddingSize];
};
/**
 * @brief Specialization for when no padding is needed (N is multiple of cache
 * line)
 */
template <std::size_t N>
struct CachelinePad<N, std::enable_if_t<(N % kCacheLineSize) == 0>> {
    static constexpr std::size_t kPaddingSize = 0;
    // Empty struct, no padding needed
};

/**
 * @brief Full cache line padding (unconditional)
 *
 * Usage:
 *   struct MyStruct {
 *       int value;
 *       CachelinePadFull pad;  // Always adds full cache line of padding
 *   };
 */
struct CachelinePadFull {
    char padding[kCacheLineSize];
};

// Compile-time sanity checks (static assertions)
/*
  assert means it will check at compile time if the condition is true or not.
  static_assert is a compile-time assertion.
  it throws runtime error if the condition is false.
*/

static_assert(kCacheLineSize >= 64, "Cache line size must be at least 64 bytes");

static_assert((kCacheLineSize & (kCacheLineSize - 1)) == 0, "Cache line size must be a power of 2");

static_assert(kDestructiveInterferenceSize >= kCacheLineSize, "Destructive interference size should be at least cache line size");

static_assert(kConstructiveInterferenceSize <= kCacheLineSize, "Constructive interference size should not exceed cache line size");
/*
  alignof is a compile-time operator that returns the alignment requirement of a type.
  alignof(struct) return the number of which it's objects' address will start.
  here it means we are not gonna pad more than cache line size in one go.
*/
static_assert(alignof(CachelinePadFull) <= kCacheLineSize, "CachelinePadFull alignment must not exceed cache line size");

// Verify the padding helper works correctly
static_assert(sizeof(CachelinePad<1>) == kCacheLineSize - 1, "CachelinePad<1> should pad to fill cache line");

static_assert(sizeof(CachelinePad<kCacheLineSize>) == 0 || std::is_empty<CachelinePad<kCacheLineSize>>::value, "CachelinePad<kCacheLineSize> should be empty");

} // namespace internal
} // namespace logger

#endif // LOGGER_INTERNAL_CACHELINE_H
