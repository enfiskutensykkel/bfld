#ifndef BFLD_STRING_POOL_H
#define BFLD_STRING_POOL_H
#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "utils/table.h"


struct strings_entry
{
    uint32_t hash;      // cached hash to avoid recomputation
    uint64_t offset;    // offset into data to the string
};



/*
 * String pool implementation.
 *
 * Create a continuous table of NUL-terminated strings
 * and track their offsets.
 */
struct strings
{
    int refcnt;
    struct table table;
    char *data;
    uint64_t size;
    uint64_t capacity;
};


/*
 * Allocate a new string pool.
 */
struct strings * strings_alloc(void);


/*
 * Take a string pool reference.
 */
struct strings * strings_get(struct strings *pool);


/*
 * Release the string pool reference.
 */
void strings_put(struct strings *pool);


/*
 * Add a string to the string pool and return the offset.
 * If the string is already added, the existing offset is returned instead.
 * Returns 0 if adding the string failed, as the first entry is reserved for the empty string.
 */
uint64_t strings_add(struct strings *pool, const char *string);


/*
 * Get the string at the given offset.
 */
static inline
const char * strings_at(struct strings *pool, uint64_t offset)
{
    if (offset < pool->size) {
        return &pool->data[offset];
    }
    return NULL;
}


#ifdef __cplusplus
}
#endif
#endif
