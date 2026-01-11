#ifndef _BFLD_UTILS_ALIGN_H
#define _BFLD_UTILS_ALIGN_H
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

    if (value > UINT64_MAX - mask) {
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

    // FIXME: fall back to some generic method if this available
    return (uint64_t) 1ULL << (64 - __builtin_clzll(value - 1));
}


#ifdef __cplusplus
}
#endif
#endif
