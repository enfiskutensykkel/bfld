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
#include <stdatomic.h>


/*
 * Take the writer lock.
 */
static inline
void globals_wrlock(struct globals *g)
{
    int32_t v = 0;

    while (!atomic_compare_exchange_weak(&g->rwlock, &v, -1)) {
        v = 0;
#if defined(__x86_64__) || defined(__i386__)
        __asm__ __volatile__("pause");
#elif defined(__aarch64__)
        __asm__ __volatile__("yield");
#else
        thrd_yield();
#endif
    }
}


/*
 * Release the writer lock.
 */
static inline
void globals_wrunlock(struct globals *g)
{
    atomic_store(&g->rwlock, 0);
}


void globals_clear(struct globals *g)
{
    assert(atomic_load(&g->rwlock) == 0);

    for (uint64_t i = 0; g->nglobals > 0 && i < g->capacity; ++i) {
        struct global *this = &g->table[i];
        if (this->symbol != NULL) {
            symbol_put(this->symbol);
            this->symbol = NULL;
            this->dfi = 0;
            g->nglobals--;
        }
    }
    free(g->table);
    g->table = NULL;
    g->threshold = 0;
    g->capacity = 0;
}


/*
 * Extend and rehash the symbol index.
 */
static int globals_resize_index(struct globals *g, uint64_t capacity)
{
    if (capacity <= g->capacity) {
        // Index was already resized
        return 0;
    }

    // Do a naive check for overflow
    if (capacity * sizeof(struct global) < g->capacity * sizeof(struct global)) {
        return ENOMEM;
    }

    struct global *table = calloc(capacity, sizeof(struct global));
    if (table == NULL) {
        return ENOMEM;
    }

    // Rehash all entries in the old index in the new table
    for (uint64_t i = 0; i < g->capacity; ++i) {
        struct symbol *symbol = g->table[i].symbol;

        if (symbol == NULL) {
            continue;
        }

        uint64_t dfi = 0;
        uint64_t slot = symbol->hash & (capacity - 1);

        while (symbol != NULL) {
            struct global *this = &table[slot];

            if (this->symbol == NULL || dfi > this->dfi) {
                struct global tmp = *this;
                this->symbol = symbol;
                this->dfi = dfi;
                symbol = tmp.symbol;
                dfi = tmp.dfi;
            }

            slot = (slot + 1) & (capacity - 1);
            ++dfi;
        }
    }

    // Release the old table and update properties
    free(g->table);
    g->table = table;
    g->capacity = capacity;
    g->threshold = GLOBALS_REHASH_THRESHOLD(capacity);

    return 0;
}


int globals_insert_symbol(struct globals *g, 
                          struct symbol *symbol,
                          struct symbol **existing)
{
    globals_rdlock(g);
    struct symbol *e = globals_find_symbol_unlocked(g, symbol->hash, symbol->name);
    globals_rdunlock(g);
    if (e != NULL) {
        if (existing != NULL) {
            *existing = e;
        }
        return EEXIST;
    }

    globals_wrlock(g);
    e = globals_find_symbol_unlocked(g, symbol->hash, symbol->name);
    if (e != NULL) {
        globals_wrunlock(g);
        if (existing != NULL) {
            *existing = e;
        }
        return EEXIST;
    }

    if (g->nglobals >= g->threshold) {
        int rc = globals_resize_index(g, g->capacity > 0 ? g->capacity * 2 : 128);
        if (rc != 0) {
            globals_wrunlock(g);
            return ENOMEM;
        }
    }

    struct global entry = (struct global) {
        .symbol = symbol_get(symbol),
        .dfi = 0,
    };
    uint64_t slot = entry.symbol->hash & (g->capacity - 1);

    while (entry.symbol != NULL) {
        struct global *this = &g->table[slot];

        if (this->symbol == NULL || entry.dfi > this->dfi) {
            struct global tmp = *this;
            *this = entry;
            entry = tmp;
        }

        slot = (slot + 1) & (g->capacity - 1);
        entry.dfi++;
    }

    g->nglobals++;
    globals_wrunlock(g);
    return 0;
}


/*
void globals_remove_symbol(struct globals *g, const struct symbol *symbol)
{
    if (g->nglobals == 0) {
        return;
    }

    const char *name = symbol->name;
    uint32_t hash = symbol->hash;

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
*/


