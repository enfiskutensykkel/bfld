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
#include <utils/stringintern.h>


struct archives * archives_alloc(void)
{
    struct archives *index = malloc(sizeof(struct archives));
    if (index == NULL) {
        return NULL;
    }

    index->refcnt = 1;
    index->capacity = 0;
    index->entries = 0;
    index->threshold = 0;
    index->table = NULL;
    memset(&index->stringpool, 0, sizeof(struct strings));
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

    capacity = align_roundup(capacity); // make sure cacpacity is a power of two

    // Do a naive overflow check
    if (capacity * sizeof(struct archive_entry) < index->capacity * sizeof(struct archive_entry)) {
        return false;
    }

    struct archive_entry *table = calloc(capacity, sizeof(struct archive_entry));
    if (table == NULL) {
        return false;
    }

    // Move entries from the old table to the new
    if (index->table != NULL) {
        for (uint64_t i = 0; i < index->capacity; ++i) {
            struct archive_entry *entry = &index->table[i];
            if (entry->hash == 0) {
                continue;
            }

            // We do a naive slot search, since we know there are no duplicates
            uint64_t slot = entry->hash & (capacity - 1);
            while (table[slot].hash != 0) {
                slot = (slot + 1) & (capacity - 1);
            }
            table[slot] = *entry;
        }
        free(index->table);
    }

    index->table = table;
    index->capacity = capacity;
    index->threshold = (index->capacity / 4) * 3;
    return true;
}


// TODO: consider using robin hood hashing
bool archives_insert_symbol(struct archives *index, 
                            struct archive_member *member,
                            const char *name)
{
    uint64_t hash = hash_fnv1a_64(name, strlen(name));
    if (hash == 0) {
        hash = 1;
    }

    if (index->entries >= index->threshold || index->entries == index->capacity) {
        if (!archives_rehash_symbols(index, index->capacity > 0 ? index->capacity * 2 : 8)) {
            return false;
        }
    }

    uint64_t slot = hash & (index->capacity - 1);

    while (index->table[slot].hash != 0) {
        if (index->table[slot].hash == hash) {
            const char *value = strings_at(&index->stringpool, index->table[slot].name);
            if (strcmp(name, value) == 0) {
                // entry was already added
                // fake that we added the symbol and keep old definition
                //log_trace("Ignoring already added symbol '%s'", name);
                return true;
            }
        }

        slot = (slot + 1) & (index->capacity - 1);
    }

    struct archive_entry *entry = &index->table[slot];
    uint64_t offset = strings_intern(&index->stringpool, name);
    if (offset == 0) {
        return false;
    }
    entry->name = offset;

    entry->hash = hash;
    entry->archive = archive_get(member->archive);
    entry->member = member;
    index->entries++;
    return true;
}


// TODO: consider using robin hood hashing
struct archive_member * archives_find_symbol(const struct archives *index,
                                             const char *name)
{
    if (index->capacity == 0) {
        return NULL;
    }

    uint64_t hash = hash_fnv1a_64(name, strlen(name));
    if (hash == 0) {
        hash = 1;
    }

    uint64_t slot = hash & (index->capacity - 1);

    while (index->table[slot].hash != 0) {
        const char *value = strings_at(&index->stringpool, index->table[slot].name);
        if (strcmp(name, value) == 0) {
            return index->table[slot].member;
        }

        slot = (slot + 1) & (index->capacity - 1);
    }

    return NULL;
}


void archives_clear_symbols(struct archives *index)
{
    if (index->table != NULL) {
        for (uint64_t i = 0; index->entries > 0 && i < index->capacity; ++i) {
            struct archive_entry *entry = &index->table[i];
            if (entry->hash != 0) {
                archive_put(entry->archive);
                entry->hash = 0;
                entry->name = 0;
                entry->archive = NULL;
                entry->member = NULL;
                index->entries--;
            }
        }
        free(index->table);
        index->table = NULL;
    }

    strings_clear(&index->stringpool);
    index->capacity = 0;
    index->entries = 0;
    index->table = NULL;
    index->threshold = 0;
}
