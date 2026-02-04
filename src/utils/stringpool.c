#include "stringpool.h"
#include "align.h"
#include "hash.h"
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>


bool string_pool_rehash(struct string_pool *pool, uint64_t capacity)
{
    if (capacity <= pool->capacity) {
        return true;
    }

    if (capacity < 8) {
        capacity = 8;
    }

    // Make sure capacity is aligned to a power of two
    capacity = align_roundup(capacity);

    // Naive check for overflow
    if (capacity * sizeof(struct intern) < pool->capacity * sizeof(struct intern)) {
        return false;
    }

    struct intern *index = (struct intern*) calloc(capacity, sizeof(struct intern));
    if (index == NULL) {
        return false;
    }

    for (uint64_t i = 0; i < pool->capacity; ++i) {
        const struct intern *entry = &pool->index[i];
        uint32_t hash = entry->hash;
        uint32_t dfi = 0;
        uint64_t offset = entry->offset;
        uint64_t slot = hash & (capacity - 1);
        
        while (hash != 0) {
            struct intern *this = &index[slot];

            if (this->hash == 0 || dfi > this->dfi) {
                uint64_t tmp_offset = this->offset;
                uint32_t tmp_hash = this->hash;
                uint32_t tmp_dfi = this->dfi;

                this->hash = hash;
                this->dfi = dfi;
                this->offset = offset;

                offset = tmp_offset;
                hash = tmp_hash;
                dfi = tmp_dfi;
            }

            slot = (slot + 1) & (capacity - 1);
            ++dfi;
        }
    }

    free(pool->index);
    pool->index = index;
    pool->capacity = capacity;
    pool->rehash_threshold = STRING_POOL_REHASH_THRESHOLD(capacity);

    return true;
}


bool string_pool_extend(struct string_pool *pool, size_t length)
{
    if (pool->size - pool->offset >= length) {
        return true;
    }

    // Naive check for overflow
    if (pool->offset + length < pool->offset) {
        return false;
    }

    uint64_t size = pool->size > 0 ? pool->size * 2 : 256;
    if (size < pool->offset + length) {
        size = align_pow2(pool->offset + length);
    }

    char *strings = (char*) realloc(pool->strings, size);
    if (strings == NULL) {
        return false;
    }

    pool->strings = strings;
    pool->size = size;
    return true;
}


void string_pool_clear(struct string_pool *pool)
{
    if (pool->index != NULL) {
        free(pool->index);
    }
    pool->index = NULL;
    pool->capacity = 0;
    pool->count = 0;
    pool->rehash_threshold = 0;

    if (pool->strings != NULL) {
        free(pool->strings);
    }
    pool->strings = NULL;
    pool->size = 0;
    pool->offset = 0;
}
