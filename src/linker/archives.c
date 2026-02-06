#include "archives.h"
#include "archive.h"
#include "logging.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <utils/hash.h>
#include <utils/align.h>
#include <utils/stringpool.h>


struct archives * archives_alloc(void)
{
    struct archives *index = malloc(sizeof(struct archives));
    if (index == NULL) {
        return NULL;
    }

    index->archives = NULL;
    index->refcnt = 1;
    index->capacity = 0;
    index->entries = 0;
    index->rehash_threshold = 0;
    index->index = NULL;
    index->names = STRING_POOL_INIT;
    index->narchives = 0;
    return index;
}


struct archives * archives_get(struct archives *index)
{
    assert(index != NULL);
    assert(index->refcnt > 0);
    index->refcnt++;
    return index;
}


void archives_put(struct archives *index)
{
    assert(index != NULL);
    assert(index->refcnt > 0);

    if (--(index->refcnt) == 0) {
        archives_clear_symbols(index);
        free(index);
    }
}


static bool archives_rehash_symbols(struct archives *index, uint64_t capacity)
{
    if (capacity <= index->capacity) {
        return true;
    }

    // Make sure cacpacity is a power of two
    capacity = align_roundup(capacity); 

    // Do a naive overflow check
    if (capacity * sizeof(struct archive_symbol) < index->capacity * sizeof(struct archive_symbol)) {
        return false;
    }

    struct archive_symbol *ht = calloc(capacity, sizeof(struct archive_symbol));
    if (ht == NULL) {
        return false;
    }

    // Move entries from the old table to the new
    for (uint64_t i = 0; i < index->capacity; ++i) {
        struct archive_symbol entry = index->index[i];

        if (entry.hash == 0) {
            continue;
        }

        entry.dfi = 0;
        uint64_t slot = entry.hash & (capacity - 1);
        
        while (entry.hash != 0) {
            struct archive_symbol *current = &ht[slot];

            if (current->hash == 0 || entry.dfi > current->dfi) {
                struct archive_symbol tmp = *current;
                *current = entry;
                entry = tmp;
            }

            slot = (slot + 1) & (capacity - 1);
            entry.dfi++;
        }
    }

    free(index->index);
    index->index = ht;
    index->capacity = capacity;
    index->rehash_threshold = (index->capacity / 4) * 3;
    return true;
}


static bool archives_add_archive(struct archives *index, struct archive *archive)
{
    uint64_t low = 0;

    if (index->narchives > 0) {
        uint64_t high = index->narchives - 1;

        while (low <= high) {
            uint64_t mid = low + (high - low) / 2;

            if (index->archives[mid] == archive) {
                return true;
            } else if (index->archives[mid] < archive) {
                low = mid + 1;
            } else {
                high = mid - 1;
            }
        }
    }

    struct archive **a = (struct archive**) realloc(index->archives, sizeof(struct archive*) * (index->narchives + 1));
    if (a == NULL) {
        return false;
    }

    index->archives = a;

    for (uint64_t i = index->narchives; i > low; --i) {
        index->archives[i] = index->archives[i-1];
    }
   
    index->archives[low] = archive_get(archive); 
    index->narchives++;
    return true;
}


bool archives_insert_symbol(struct archives *index, struct archive_member *member, const char *symbol_name)
{
    struct archive *archive = member->archive;
    uint32_t hash = hash_fnv1a_32(symbol_name, strlen(symbol_name));
    if (hash == 0) {
        hash = 1;
    }
    
    if (index->entries >= index->rehash_threshold || index->entries == index->capacity) {
        if (!archives_rehash_symbols(index, index->capacity > 0 ? index->capacity * 2 : 64)) {
            return false;
        }
    }

    bool inserted = false;
    uint64_t mask = index->capacity - 1;
    uint64_t slot = hash & mask;
    uint32_t dfi = 0;
    uint64_t name = 0;

    while (hash != 0) {
        struct archive_symbol *current = &index->index[slot];

        if (!inserted && current->hash == hash) {
            const char *existing = string_pool_at(&index->names, current->name);
            if (strcmp(symbol_name, existing) == 0) {
                return true;
            }
        }

        if (current->hash == 0 || dfi > current->dfi) {

            if (!inserted) {
                if (!archives_add_archive(index, archive)) {
                    return false;
                }

                name = string_pool_intern(&index->names, symbol_name);
                if (name == 0) {
                    return false;
                }

                inserted = true;
            }

            struct archive_symbol tmp = *current;
            current->hash = hash;
            current->dfi = dfi;
            current->name = name;
            current->member = member;

            hash = tmp.hash;
            dfi = tmp.dfi;
            name = tmp.name;
            member = tmp.member;
        }

        slot = (slot + 1) & mask;
        ++dfi;
    }

    index->entries++;
    return true;
}


struct archive_member * 
archives_find_symbol(const struct archives *index, const char *symbol_name)
{
    if (index->capacity == 0) {
        return NULL;
    }

    uint32_t hash = hash_fnv1a_32(symbol_name, strlen(symbol_name));
    if (hash == 0) {
        hash = 1;
    }
    uint64_t mask = index->capacity - 1;
    uint64_t slot = hash & mask;
    uint32_t dfi =  0;

    const struct archive_symbol *this = &index->index[slot];

    while (this->hash != 0 && dfi <= this->dfi) {
        if (this->hash == hash) {
            const char *existing = string_pool_at(&index->names, this->name);
            if (strcmp(existing, symbol_name) == 0) {
                return this->member;
            }
        }

        slot = (slot + 1) & mask;
        this = &index->index[slot];
        ++dfi;
    }

    return NULL;
}


void archives_clear_symbols(struct archives *index)
{
    if (index->archives != NULL) {
        for (uint64_t i = 0; i < index->narchives; ++i) {
            struct archive *ar = index->archives[i];
            archive_put(ar);
        }
        index->narchives = 0;
        free(index->archives);
        index->archives = NULL;
    }

    string_pool_clear(&index->names);
    index->capacity = 0;
    index->entries = 0;
    free(index->index);
    index->index = NULL;
    index->rehash_threshold = 0;
}
