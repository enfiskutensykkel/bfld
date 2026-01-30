#ifndef BFLD_UTILS_ALIGN_H
#define BFLD_UTILS_ALIGN_H
#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>


/* 
 * Helper function for aligning addresses and sizes to a specified alignment.
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
 * Helper function to round up to nearest power of 2.
 */
static inline 
uint64_t align_pow2(uint64_t value) 
{
    if (value <= 1) {
        return 1;
    }

#if defined(__has_builtin) && __has_builtin(__builtin_clzll)
    return (uint64_t) 1ULL << (64 - __builtin_clzll(value - 1));
#elif defined(__GNUC__) && ((__GNUC__ > 3) || (__GNUC__ == 3 && __GNUC_MINOR__ >= 4))
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
 * Helper function to calculate the ceiling of the binary 
 * logarithm of value.
 */
static inline
uint8_t align_ceillog2(uint64_t value)
{
    if (value <= 1) {
        return 0;
    }

#if defined(__has_builtin) && __has_builtin(__builtin_clzll)
    return (uint8_t) (64 - __builtin_clzll(value - 1));
#elif defined(__GNUC__) && ((__GNUC__ > 3) || (__GNUC__ == 3 && __GNUC_MINOR__ >= 4))
    return (uint8_t) (64 - __builtin_clzll(value - 1));
#else

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
 * Helper fuction to get the exponent of a number
 * that is known to be a power of 2 (i.e., n of 2^n).
 */
static inline
uint8_t align_pow2exp(uint64_t value)
{
    if (value == 0) {
        return 0;
    }

#if defined(__has_builtin) && __has_builtin(__builtin_ctzll)
    return (uint8_t) __builtin_ctzll(value);
#else
    uint8_t count = 0;
    uint64_t v = value;

    while ((v & 1) == 0) {
        v >>= 1;
        ++count;
    }
    return count;
#endif
}


#ifdef __cplusplus
}
#endif
#endif
