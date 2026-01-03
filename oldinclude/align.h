#ifndef __BFLD_ALIGN_H__
#define __BFLD_ALIGN_H__

#include <stdint.h>


/* 
 * Convenience macro for aligning addresses and sizes to a specified alignment.
 * Alignment must be a power of two and greater than 0.
 */
#define BFLD_ALIGN(size, alignment) \
    (((uint64_t) (size) + (alignment) - 1) & ~(((uint64_t) (alignment) - 1)))


#endif
