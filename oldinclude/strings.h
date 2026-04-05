#ifndef BFLD_STRING_INTERNING_H
#define BFLD_STRING_INTERNING_H
#ifdef __cplusplus
extern "C" {
#endif

#include <strings.h>
#include "htable.h"
#include "hash.h"


struct strings
{
    struct htable hashtable;
};


static inline
void strings_init(struct strings *pool, size_t nstrings)
{
    htable_init(&pool->hashtable, nstrings * 2);
}


static inline
void strings_free(struct strings *pool)
{
    htable_free(&pool->hashtable);
}


// FIXME: maybe this should live directly on linkerctx
static inline
const char * strings_intern(struct strings *pool, const char *string)
{
    size_t length = strlen(string) + 1;
    uint64_t hash = hash_xxh_32(string, length);

    return htable_put(&pool->hashtable, hash, string, length, (void*) string);
}


#ifdef __cplusplus
}
#endif
#endif
