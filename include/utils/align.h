#ifndef BFLD_UTILS_ALIGN_H
#define BFLD_UTILS_ALIGN_H
#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>


#if ((defined(__has_builtin) && __has_builtin(__builtin_clzll)) \
        || (defined(__GNUC__) && ((__GNUC__ > 3) || (__GNUC__ == 3 && __GNUC_MINOR__ >= 4))))
#define HAS_CLZ 1
#else
#define HAS_CLZ 0
#endif


/* 
 * Helper function for aligning addresses and sizes to a specified alignment.
 * Alignment must be a power of two.
 */
static inline
uint64_t align_to(uint64_t value, uint64_t alignment)
{
    if (alignment <= 1) {
        return value;
    }

    uint64_t mask = alignment - 1;

    if (value > (UINT64_MAX - mask)) {
        return UINT64_MAX;
    }

    return (value + mask) & ~mask;
}


/*
 * Helper function for aligning addresses and sizes down to a specified alignment.
 * Alignment must be a power of two.
 */
static inline
uint64_t align_clamp(uint64_t value, uint64_t alignment)
{
    if (alignment <= 1) {
        return value;
    }

    uint64_t mask = alignment - 1;
    return value & ~mask;
}


/*
 * Helper function to round up to nearest power of two.
 */
static inline 
uint64_t align_roundup(uint64_t value) 
{
    if (value <= 1) {
        return 1;
    }

    if (value > 0x8000000000000000ULL) {
        return 0;
    }

#if HAS_CLZ
    return (uint64_t) 1ULL << (64 - __builtin_clzll(value - 1));
#else
    // Fallback to bit-smearing algorithm
    uint64_t v = value - 1;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v |= v >> 32;
    return v + 1;
#endif
}


/*
 * Helper function to calculate the ceiling of the binary logarithm of value,
 * or ceil(log2(value)). 
 *
 * Note that for exact powers of two, ceil(log2(x)) == floor(log2(x))
 */
static inline
uint8_t align_ceillog2(uint64_t value)
{
    if (value <= 1) {
        return 0;
    }

#if HAS_CLZ
    return (uint8_t) (64 - __builtin_clzll(value - 1));
#else
    // Fallback to simple bit shifting loop
    uint8_t count = 0;
    uint64_t v = value - 1;

    while (v > 0) {
        v >>= 1;
        ++count;
    }

    return count;
#endif
}


/*
 * Helper function to calculate the floor of the binary logarithm of value,
 * or floor(log2(value)).
 *
 * Note that for exact powers of two, floor(log2(x)) == ceil(log2(x))
 */
static inline
uint8_t align_floorlog2(uint64_t value)
{
    if (value <= 1) {
        return 0;
    }
#if HAS_CLZ
    return (uint8_t) 63 - __builtin_clzll(value);
#else
    // Fallback to simple bit shifting loop
    uint8_t count = 0;
    uint64_t v = value;

    while (v > 0) {
        v >>= 1;
        ++count;
    }

    return count - 1;
#endif
}


/*
 * Calculate the integer binary logarithm of value.
 * This is the same as floor(log2(value)).
 */
#define align_log2(value) align_floorlog2(value)


/*
 * Calculate the n'th power of two (2^n).
 */
#define align_pow2(n) (1ULL << (n))


/*
 * Calculate if value is a power of two.
 */
#define align_is_pow2(value) ((((uint64_t) (value)) != 0) && (((uint64_t) (value)) & (((uint64_t) (value)) - 1)) == 0)


#ifdef __cplusplus
}
#endif
#endif
