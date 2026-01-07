#ifndef _BFLD_UTILS_ALIGN_H
#define _BFLD_UTILS_ALIGN_H
#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>


/* 
 * Convenience macro for aligning addresses and sizes to a specified alignment.
 */
#define align_to(size, alignment) \
    (((uint64_t) (size) + (1ULL << (alignment)) - 1) & ~(((uint64_t) (1ULL << (alignment)) - 1)))


/*
 * Helper function to round up to nearest power of 2.
 */
static inline 
uint64_t align_pow2(uint64_t v) 
{
    if (v <= 1) {
        return 1;
    }

    // FIXME: fall back to some generic method if this available
    return (uint64_t) 1ULL << (64 - __builtin_clzll(v - 1));
}


#ifdef __cplusplus
}
#endif
#endif
