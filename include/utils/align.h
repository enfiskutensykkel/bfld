#ifndef __BFLD_UTILS_ALIGN_H__
#define __BFLD_UTILS_ALIGN_H__

#include <stdint.h>


/* 
 * Convenience macro for aligning addresses to a specified alignment.
 * Alignment must be a power of two.
 */
#define ALIGN_ADDR(size, alignment) \
    (((uint64_t) (size) + (alignment) - 1) & ~(((uint64_t) (alignment) - 1)))


#endif
