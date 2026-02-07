#include "globals.h"
#include "symbol.h"
#include "logging.h"
#include "utils/hash.h"
#include "utils/align.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>


static bool rehash(struct globals *g, uint64_t capacity)
{
    if (capacity < 128) {
        capacity = 128;
    }

    if (capacity <= g->capacity) {
        return true;
    }

    // Round capacity up to a power of two and make sure we don't overflow
    capacity = align_roundup(capacity);
    if (capacity * sizeof(struct global) < g->capacity * sizeof(struct global)) {
        return false;
    }

    struct global *table = (struct global*) calloc(capacity, sizeof(struct global));
    if (table == NULL) {
        return false;
    }

    for (uint64_t i = 0; i < g->capacity; ++i) {
        struct global current = g->table[i];

        if (current.hash == 0) {
            continue;
        }

        current.dfi = 0;
        uint64_t slot = current.hash & (capacity - 1);

        while (current.hash != 0) {
            struct global *this = &table[slot];

            if (this->hash == 0 || current.dfi > this->dfi) {
                struct global tmp = *this;
                *this = current;
                current = tmp;
            }

            slot = (slot + 1) & (capacity - 1);
            current.dfi++;
        }
    }

    free(g->table);
    g->table = table;
    g->capacity = capacity;
    g->rehash_threshold = GLOBALS_REHASH_THRESHOLD(capacity);
    return true;
}


int globals_insert_symbol(struct globals *g, 
                          struct symbol *symbol,
                          struct symbol **existing)
{
    const char *name = symbol_name(symbol);

    struct symbol *e = globals_find_symbol(g, name);
    if (e != NULL) {
        if (existing != NULL) {
            *existing = e;
        }
        return EEXIST;
    }

    size_t length = strlen(name);
    uint32_t hash = hash_fnv1a_32(name, length);
    if (hash == 0) {
        hash = 1;
    }

    if (g->nglobals >= g->rehash_threshold) {
        uint64_t capacity = g->capacity > 0 ? g->capacity * 2 : 128;
        if (!rehash(g, capacity)) {
            return ENOMEM;
        }
    }

    struct global entry = (struct global) {
        .hash = hash,
        .dfi = 0,
        .symbol = symbol
    };
    uint64_t slot = entry.hash & (g->capacity - 1);

    while (entry.hash != 0) {
        struct global *this = &g->table[slot];

        if (this->hash == 0 || entry.dfi > this->dfi) {
            struct global tmp = *this;
            *this = entry;
            if (hash != 0) {
                hash = 0;
                this->symbol = symbol_get(entry.symbol);
                g->nglobals++;
            }
            entry = tmp;
        }

        slot = (slot + 1) & (g->capacity - 1);
        entry.dfi++;
    }

    return 0;
}


void globals_remove_symbol(struct globals *g, const struct symbol *symbol)
{
    if (g->nglobals == 0) {
        return;
    }

    const char *name = symbol_name(symbol);
    size_t length = strlen(name);
    uint32_t hash = hash_fnv1a_32(name, length);
    if (hash == 0) {
        hash = 1;
    }

    uint64_t slot = hash & (g->capacity - 1);
    struct global *this = &g->table[slot];
    uint64_t dfi = 0;

    while (this->hash != 0 && dfi <= this->dfi) {
        if (this->hash == hash && this->symbol == symbol) {
            symbol_put(this->symbol);
            break;
        }
        slot = (slot + 1) & (g->capacity - 1);
        this = &g->table[slot];
        ++dfi;
    }
    
    if (this->hash != 0 && dfi <= this->dfi) {
        g->nglobals--;

        struct global *next = &g->table[(slot + 1) & (g->capacity - 1)];
        while (next->hash != 0 && next->dfi != 0) {
            *this = *next;
            this->dfi--;
            this = next;
        }

        this->hash = 0;
        this->dfi = 0;
        this->symbol = NULL;
    }
}


void globals_clear(struct globals *g)
{
    for (uint64_t i = 0; g->nglobals > 0 && i < g->capacity; ++i) {
        struct global *this = &g->table[i];
        if (this->hash != 0) {
            symbol_put(this->symbol);
            g->nglobals--;
        }
    }
    free(g->table);
    g->table = NULL;
    g->rehash_threshold = 0;
    g->capacity = 0;
}
