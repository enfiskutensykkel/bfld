#include "archive_index.h"
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


// strdup is a POSIX call
extern char * strdup(const char *s);


struct archive_index * archive_index_alloc(void)
{
    struct archive_index *index = malloc(sizeof(struct archive_index));
    if (index == NULL) {
        return NULL;
    }

    index->refcnt = 1;
    index->capacity = 0;
    index->entries = 0;
    index->threshold = 0;
    index->table = NULL;
    return index;
}


struct archive_index * archive_index_get(struct archive_index *index)
{
    assert(index != NULL);
    assert(index->refcnt > 0);
    index->refcnt++;
    return index;
}


void archive_index_put(struct archive_index *index)
{
    assert(index != NULL);
    assert(index->refcnt > 0);

    if (--(index->refcnt) == 0) {
        archive_index_clear(index);
        free(index);
    }
}


bool archive_index_insert(struct archive_index *index, 
                          struct archive_member *member,
                          const char *name)
{
    uint32_t hash = hash_fnv1a(name);
    if (hash == 0) {
        hash = 1;
    }

    if (index->entries >= index->threshold || index->entries == index->capacity) {
        if (!archive_index_rehash(index, index->capacity > 0 ? index->capacity * 2 : 8)) {
            return false;
        }
    }

    uint64_t slot = hash & (index->capacity - 1);

    while (index->table[slot].hash != 0) {
        if (index->table[slot].hash == hash) {
            if (strcmp(name, index->table[slot].name) == 0) {
                // entry was already added
                // fake that we added the symbol and keep old definition
                //log_trace("Ignoring already added symbol '%s'", name);
                return true;
            }
        }

        slot = (slot + 1) & (index->capacity - 1);
    }

    struct archive_entry *entry = &index->table[slot];
    entry->name = strdup(name);  // FIXME: use string pool in future
    if (entry->name == NULL) {
        return false;
    }

    entry->hash = hash;
    entry->archive = archive_get(member->archive);
    entry->member = member;
    index->entries++;
    return true;
}


bool archive_index_rehash(struct archive_index *index, uint64_t capacity)
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


struct archive_member * archive_index_find(const struct archive_index *index,
                                           const char *name)
{
    if (index->capacity == 0) {
        return NULL;
    }

    uint32_t hash = hash_fnv1a(name);
    if (hash == 0) {
        hash = 1;
    }

    uint64_t slot = hash & (index->capacity - 1);

    while (index->table[slot].hash != 0) {
        if (strcmp(name, index->table[slot].name) == 0) {
            return index->table[slot].member;
        }

        slot = (slot + 1) & (index->capacity - 1);
    }

    return NULL;
}


void archive_index_clear(struct archive_index *index)
{
    for (uint64_t i = 0; index->entries > 0 && i < index->capacity; ++i) {
        // FIXME: if we use a string pool (and each entries just point to the offset), cleanup becomes O(1)
        struct archive_entry *entry = &index->table[i];

        if (entry->hash != 0) {
            free(entry->name);
            entry->name = NULL;
            entry->member = NULL;
            archive_put(entry->archive);
            entry->archive = NULL;
            entry->hash = 0;
            index->entries--;
        }
    }
    index->capacity = 0;
    index->entries = 0;
    free(index->table);
    index->table = NULL;
    index->threshold = 0;
}
