#include "stringintern.h"
#include "align.h"
#include "hash.h"
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>


bool strings_resize_table(struct strings *pool, uint64_t capacity)
{
    if (capacity <= pool->table_capacity) {
        return true;
    }

    // Make sure capacity is aligned to a power of two
    capacity = align_roundup(capacity);
    if (capacity < 8) {
        capacity = 8;
    }

    // Naive check for overflow
    if (capacity * sizeof(struct intern) < pool->table_capacity * sizeof(struct intern)) {
        return false;
    }

    struct intern *table = (struct intern*) calloc(capacity, sizeof(struct intern));
    if (table == NULL) {
        return false;
    }

    for (uint64_t i = 0; i < pool->table_capacity; ++i) {
        const struct intern *entry = &pool->table[i];
        uint64_t slot = entry->hash & (capacity - 1);
        uint64_t hash = entry->hash;
        uint64_t offset = entry->offset;
        uint64_t dfi = 0;

        while (hash != 0) {
            struct intern *this = &table[slot];

            if (this->hash == 0 || dfi > this->dfi) {
                uint64_t tmp_hash = this->hash;
                uint64_t tmp_offset = this->offset;
                uint64_t tmp_dfi = this->dfi;
                this->hash = hash;
                this->offset = offset;
                this->dfi = dfi;
                hash = tmp_hash;
                offset = tmp_offset;
                dfi = tmp_dfi;
            }

            slot = (slot + 1) & (capacity - 1);
            dfi++;
        }
    }

    free(pool->table);
    pool->table = table;
    pool->table_capacity = capacity;
    pool->rehash_threshold = (capacity / 4) * 3;

    return true;
}


bool strings_reserve_bytes(struct strings *pool, size_t length)
{
    if (pool->data_capacity - pool->data_size >= length) {
        return true;
    }

    // Naive check for overflow
    if (pool->data_size + length < pool->data_size) {
        return false;
    }

    uint64_t capacity = pool->data_capacity > 0 ? pool->data_capacity * 2 : 256;
    if (capacity < pool->data_size + length) {
        capacity = pool->data_size + length;
    }

    char *data = (char*) realloc(pool->data, capacity);
    if (data == NULL) {
        return false;
    }

    if (pool->data_capacity == 0) {
        data[0] = '\0';
        pool->data_size = 1;
    }

    pool->data = data;
    pool->data_capacity = capacity;
    return true;
}


void strings_clear(struct strings *pool)
{
    if (pool->table != NULL) {
        free(pool->table);
    }
    pool->table = NULL;
    pool->table_capacity = 0;
    pool->table_size = 0;
    pool->rehash_threshold = 0;

    if (pool->data != NULL) {
        free(pool->data);
    }
    pool->data = NULL;
    pool->data_capacity = 0;
    pool->data_size = 0;
}
