#ifndef BFLD_UTILS_STRING_INTERN_H
#define BFLD_UTILS_STRING_INTERN_H
#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "hash.h"


/*
 * String intern.
 *
 * Contains the hash and the offset in
 * the underlying pool to the actual string.
 */
struct intern
{
    uint64_t hash;      // computed hash of the string (0 means unused)
    uint64_t dfi;       // distance from ideal (for robin hood hashing)
    uint64_t offset;    // offset into data to the string value
};



/*
 * String pool implementation.
 *
 * Create a continuous table of NUL-terminated strings
 * and track their offsets.
 */
struct strings
{
    char *data;                 // underlying string pool data
    uint64_t data_size;         // total size of all strings in the pool
    uint64_t data_capacity;     // capacity of the string pool
    uint64_t table_size;        // number of strings in the pool
    uint64_t table_capacity;    // capacity of the hash table (must be power of 2)
    struct intern *table;       // hash table
    uint64_t rehash_threshold;  // threshold for when to expand and rehash (current capacity * load factor)
};


/*
 * Clear the string pool entirely.
 */
void strings_clear(struct strings *pool);


/*
 * Extend and rehash the string pool to the new capacity.
 */
bool strings_resize_table(struct strings *pool, uint64_t table_capacity);


/*
 * Grow the underlying pool data.
 */
bool strings_reserve_bytes(struct strings *pool, size_t length);


/*
 * Add a string to the string pool and return the offset.
 *
 * If the string is already added, the existing offset is returned instead.
 * Returns 0 if adding the string failed, as the first entry is reserved for the empty string.
 */
static inline
uint64_t strings_intern(struct strings *pool, const char *string)
{
    if (string == NULL || string[0] == '\0') {
        return 0;
    }

    size_t length = strlen(string);
    uint64_t hash = hash_fnv1a_64(string, length);

    if (hash == 0) {
        hash = 1;  // index 0 is reserved for the empty string
    }

    if (pool->table_size >= pool->rehash_threshold) {
        if (!strings_resize_table(pool, pool->table_capacity > 0 ? pool->table_capacity * 2 : 8)) {
            return 0;
        }
    }

    uint64_t mask = (pool->table_capacity - 1);
    uint64_t slot = hash & mask;
    uint64_t offset = 0;
    uint64_t result = 0;
    uint64_t dfi = 0;

    while (hash != 0) {
        struct intern *this = &pool->table[slot];

        uint64_t tmp_hash = this->hash;
        uint64_t tmp_offset = this->offset;
        uint64_t tmp_dfi = this->dfi;

        if (result == 0 && tmp_hash == hash) {
            const char *value = &pool->data[this->offset];
            if (strcmp(string, value) == 0) {
                return this->offset;
            }
        }

        if (tmp_hash == 0 || dfi > this->dfi) {
            if (result == 0) {
                if (pool->data_capacity - pool->data_size < length + 2) {
                    if (!strings_reserve_bytes(pool, length + 2)) {
                        return 0;
                    }
                }

                result = offset = pool->data_size + 1;
                memcpy(&pool->data[offset], string, length + 1);
                pool->data_size += length + 1;
                pool->table_size++;
            }

            this->hash = hash;
            this->offset = offset;
            this->dfi = dfi;

            hash = tmp_hash;
            offset = tmp_offset;
            dfi = tmp_dfi;
        }

        slot = (slot + 1) & mask;
        ++dfi;
    }

    return result;
}


/*
 * Look up the string in the pool and return the offset.
 * Returns 0 if not found, which is also the same as the empty string.
 */
static inline
uint64_t strings_lookup(const struct strings *pool, const char *string)
{
    if (string == NULL || string[0] == '\0') {
        return 0;
    }

    size_t length = strlen(string);
    uint64_t hash = hash_fnv1a_64(string, length);
    if (hash == 0) {
        hash = 1;
    }

    uint64_t mask = pool->table_capacity - 1;
    uint64_t slot = hash & mask;
    uint64_t dfi = 0;
    const struct intern *this = &pool->table[slot];

    while (this->hash != 0 && dfi <= this->dfi) {
        if (this->hash == hash) {
            const char *value = &pool->data[this->offset];
            if (strcmp(string, value) == 0) {
                return this->offset;
            }
        }

        slot = (slot + 1) & mask;
        this = &pool->table[slot];
        ++dfi;
    }

    return 0;
}


/*
 * Get the string at the given offset.
 */
static inline
const char * strings_at(const struct strings *pool, uint64_t offset)
{
    if (offset == 0) {
        return "";
    }

    if (offset < pool->data_size) {
        return &pool->data[offset];
    }

    return NULL;
}


#ifdef __cplusplus
}
#endif
#endif
