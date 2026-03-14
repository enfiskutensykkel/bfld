#ifndef BFLD_STRING_POOL_H
#define BFLD_STRING_POOL_H
#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "utils/hash.h"


/*
 * String interning table entry.
 *
 * Contains the hash, distance from ideal (DFI) and the offset in
 * the underlying string table (to the actual string value).
 */
struct intern
{
    uint32_t hash;      // computed hash of the string (0 means unused)
    uint32_t dfi;       // distance from ideal (for robin hood hashing)
    uint64_t offset;    // offset into the table to the string value
    size_t length;      // length of the string
};


/*
 * Calculate the rehash threshold for a given capacity.
 *
 * To minimise hash collisions, the hash table needs to 
 * be rehashed when the number of strings exceed a given load factor (in this case, 75%).
 *
 * Capacity must be a power of two.
 */
#define STRING_POOL_REHASH_THRESHOLD(capacity) (((capacity) / 4) * 3)


/*
 * String pool implementation.
 *
 * Create a continuous table of NUL-terminated strings
 * and track their offsets using a hash table as an index.
 *
 * The hash table uses Robin Hood hashing to deal
 * with collisions, and rehashes when size >= rehash_threshold.
 */
struct strpool
{
    int refcnt;                 // reference counter
    char *strings;              // underlying string table
    uint64_t offset;            // current offset in the underlying string table
    uint64_t size;              // total size of the memory region holding the string table
    uint64_t count;             // number of strings in the pool (entries in the index)
    uint64_t capacity;          // capacity of the index (must be power of 2)
    struct intern *index;       // hash table
    uint64_t rehash_threshold;  // threshold for when to expand and rehash (current capacity * load factor)
};


/*
 * Create a string pool.
 */
struct strpool * strpool_alloc(void);


/*
 * Take a string pool reference.
 */
struct strpool * strpool_get(struct strpool *pool);


/*
 * Release a string pool reference.
 */
void strpool_put(struct strpool *pool);


/*
 * Clear the string pool entirely.
 */
void strpool_clear(struct strpool *pool);


/*
 * Grow the underlying string table with at least size bvtes.
 */
bool strpool_extend(struct strpool *pool, size_t size);


/*
 * Extend and rehash the string pool to the new capacity.
 *
 * Note that this does not compact the underlying string table,
 * since offsets are expected to be stable.
 */
bool strpool_rehash(struct strpool *pool, uint64_t capacity);


/*
 * Append a string with a given length to the underlying string table.
 *
 * Note that length should include the terminating NUL-character.
 */
static inline
uint64_t strpool_raw_intern(struct strpool *pool, const char *string, size_t length)
{
    if (string == NULL || string[0] == '\0') {
        return 0;
    }

    if (pool->size - pool->offset <= length) {
        if (!strpool_extend(pool, length)) {
            return 0;
        }
    }

    uint64_t offset = pool->offset;
    memcpy(&pool->strings[offset], string, length);
    pool->offset += length;
    return offset;
}


/*
 * Add a string to the string pool and return the offset.
 *
 * If the string is already added, the existing offset is returned.
 * Returns 0 if adding the string failed, as the first entry is reserved for the empty string.
 */
static inline
uint64_t strpool_intern(struct strpool *pool, const char *string)
{
    if (string == NULL || string[0] == '\0') {
        return 0;
    }

    size_t length = strlen(string) + 1;
    uint32_t hash = hash_fnv1a_32(string, length - 1);
    if (hash == 0) {
        hash = 1;  // hash == 0 means unused
    }

    // Check if entry already exists
    uint64_t mask = pool->capacity - 1;
    uint64_t slot = hash & mask;
    uint32_t dfi = 0;

    if (pool->count > 0) {
        while (pool->index[slot].hash != 0 && pool->index[slot].dfi <= dfi) {
            if (pool->index[slot].hash == hash) {
                uint64_t offset = pool->index[slot].offset;
                const char *value = &pool->strings[offset];
                if (strcmp(string, value) == 0) {
                    return offset;
                }
            }
            slot = (slot + 1) & mask;
            ++dfi;
        }
    }

    // Resize and rehash if we need to
    if (pool->count >= pool->rehash_threshold) {
        if (!strpool_rehash(pool, pool->capacity > 0 ? pool->capacity * 2 : 8)) {
            return 0;
        }
    }

    mask = pool->capacity - 1;
    slot = hash & mask;
    dfi = 0;

    uint64_t result = 0;
    uint64_t offset = 0;

    while (hash != 0) {
        struct intern *this = &pool->index[slot];

        if (this->hash == 0 || dfi > this->dfi) {
            struct intern tmp = *this;

            // Found an available slot or the start of a new bucket
            if (result == 0) {
                // Insert our string into the pool data
                result = offset = strpool_raw_intern(pool, string, length);
                if (result == 0) {
                    return 0;
                }
                pool->count++;
            }

            // Update the current slot and start finding 
            // a new home for the slot we are replacing
            this->hash = hash;
            this->offset = offset;
            this->dfi = dfi;
            this->length = length;

            hash = tmp.hash;
            offset = tmp.offset;
            dfi = tmp.dfi;
            length = tmp.length;
        }

        slot = (slot + 1) & mask;
        ++dfi;
    }

    return result;
}


static inline
void strpool_unintern(struct strpool *pool, const char *string)
{
    if (string == NULL || string[0] == '\0' || pool->count == 0) {
        return;
    }

    size_t length = strlen(string) + 1;
    uint32_t hash = hash_fnv1a_32(string, length - 1);
    if (hash == 0) {
        hash = 1;  // hash == 0 means unused
    }

    uint64_t mask = pool->capacity - 1;
    uint64_t slot = hash & mask;
    uint32_t dfi = 0;
    struct intern *this = &pool->index[slot];

    while (this->hash != 0 && dfi <= this->dfi) {

        if (this->hash == hash && this->length == length) {
            const char *value = &pool->strings[this->offset];
            if (strcmp(value, string) == 0) {
                break;
            }
        }

        slot = (slot + 1) & mask;
        this = &pool->index[slot];
        ++dfi;
    }

    // Backwards shift deletion
    if (this->hash != 0 && dfi <= this->dfi) {
        pool->count--;

        struct intern *next = &pool->index[(slot + 1) & mask];
        while (next->hash != 0 && next->dfi != 0) {
            *this = *next;
            this->dfi--;
            this = next;
            slot = (slot + 1) & mask;
            next = &pool->index[(slot + 1) & mask];
        }

        this->hash = 0;
        this->dfi = 0;
        this->offset = 0;
        this->length = 0;
    }
}


/*
 * Look up the string in the pool and return the offset.
 * Returns 0 if not found, which is also the same as the empty string.
 */
static inline
uint64_t strpool_lookup(const struct strpool *pool, const char *string)
{
    if (string == NULL || string[0] == '\0') {
        return 0;
    }

    size_t length = strlen(string);
    uint32_t hash = hash_fnv1a_32(string, length);
    if (hash == 0) {
        hash = 1;  // hash == 0 means unused
    }

    uint64_t mask = pool->capacity - 1;
    uint64_t slot = hash & mask;
    uint32_t dfi = 0;
    const struct intern *this = &pool->index[slot];

    while (this->hash != 0 && dfi <= this->dfi) {
        if (this->hash == hash) {
            const char *value = &pool->strings[this->offset];
            if (strcmp(string, value) == 0) {
                return this->offset;
            }
        }

        slot = (slot + 1) & mask;
        this = &pool->index[slot];
        ++dfi;
    }

    return 0;
}


/*
 * Get the string at the given offset.
 */
static inline
const char * strpool_at(const struct strpool *pool, uint64_t offset)
{
    if (offset == 0) {
        return "";
    }

    if (offset < pool->offset) {
        return &pool->strings[offset];
    }

    return NULL;
}


/*
 * Parse a string table with NUL-terminated strings, and build a string pool. 
 * This will also perform tail merging, in order to compact the underlying 
 * string table used by the pool.
 *
 * Returns the number of strings in the string pool after merging.
 */
uint64_t strpool_pack(struct strpool *pool, const char *strtab, uint64_t size);


#define strpool_for_each_offset(iterator, pool_ptr) \
    for (uint64_t __idx = 0; __idx < (pool_ptr)->capacity; __idx++) \
        for (uint64_t __once = 1, iterator = (pool_ptr)->index[__idx].offset; __once && (pool_ptr)->index[__idx].hash != 0; __once = 0)


#ifdef __cplusplus
}
#endif
#endif
