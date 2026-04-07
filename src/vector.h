#ifndef BFLD_VECTOR_H
#define BFLD_VECTOR_H
#ifdef __cplusplus
extern "C" {
#endif

#include "cdefs.h"
#include <stddef.h>
#include "arena.h"


struct vector
{
    
};


static inline
void vector_init(struct vector *vec)


static inline
void ** vector_push(struct vector *vec, size_t size)
{
    return 0;
}


static inline
void * vector_at(struct vector *vec, uint64_t index)
{
    uint8_t segment = align_floorlog2(index + 2
}


static inline
void * vector_clear(struct vector *vec)
{
}


#ifdef __cplusplus
}
#endif
#endif
