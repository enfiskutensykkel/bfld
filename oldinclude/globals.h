#ifndef BFLD_GLOBALS_H
#define BFLD_GLOBALS_H
#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <threads.h>
#include <string.h>
#include "symbol.h"
#include "utils/hash.h"


/* Forward declaration */
struct section;
struct globals;


#define GLOBALS_REHASH_THRESHOLD(capacity) \
    (((capacity) / 4) * 3)


/*
 * Entry in the global symbol index hash table.
 * Allows tracking symbols by name.
 */
struct global
{
    struct symbol *symbol;  // strong reference to the symbol
    uint64_t dfi;           // distance from ideal in the name hash table
};


/*
 * Global symbol index.
 * Tracks global symbols by names and by sections.
 */
struct globals
{
    _Atomic int32_t rwlock;     // reader-writer lock
    struct global *table;       // hash table of global symbols
    uint64_t capacity;          // capacity of the hash table
    uint64_t nglobals;          // number of global symbols in the hash table
    uint64_t threshold;         // rehashing threshold
};


#

/*
 * Initialize the symbol index.
 */
void globals_init(struct globals *g);


/*
 * Clear the symbol index and remove all entries.
 */
void globals_clear(struct globals *g);


static inline
void globals_rdlock(struct globals *g)
{
    while (true) {
        int32_t v = atomic_load(&g->rwlock);
        if (v >= 0 && atomic_compare_exchange_weak(&g->rwlock, &v, v + 1)) {
            break;
        }
    }
}


static inline
void globals_rdunlock(struct globals *g)
{
    atomic_fetch_sub(&g->rwlock, 1);
}


/*
 * Helper function to look up a symbol from the global symbol index.
 *
 * Note that this does not take the lock. The caller must make take
 * the appropriate RWlock before calling this.
 */
static inline struct symbol * 
globals_find_symbol_unlocked(struct globals *g, uint32_t hash, const char *name)
{
    if (g->nglobals == 0) {
        return NULL;
    }

    uint64_t slot = hash & (g->capacity - 1);
    uint64_t dfi = 0;

    while (g->table[slot].symbol != NULL && dfi <= g->table[slot].dfi) {
        const struct global *this = &g->table[slot];

        if (this->symbol->hash == hash) {
            if (strcmp(name, this->symbol->name) == 0) {
                return this->symbol;
            }
        }

        slot = (slot + 1) & (g->capacity - 1);
        ++dfi;
    }

    return NULL;
}


/*
 * Look up a symbol from its name in the global symbol index.
 */
static inline
struct symbol * globals_find_symbol(struct globals *g, const char *name)
{
    uint32_t hash = hash_fnv1a_32(name, strlen(name));
    if (hash == 0) {
        hash = 1;
    }

    globals_rdlock(g);
    struct symbol *symbol = globals_find_symbol_unlocked(g, hash, name);
    globals_rdunlock(g);
    return symbol;
}


/*
 * Insert a symbol to the global symbol index.
 *
 * If the symbol's name is unique, a strong reference to
 * the symbol is taken, the symbol is inserted into the index,
 * and the function returns 0.
 *
 * If a symbol with the same name already is inserted in the symbol 
 * index, this funtion returns EEXIST. 
 *
 * If the optional existing pointer is non-NULL and a symbol with the same
 * name already is inserted, the pointer is set to the existing symbol.
 * Otherwise the existing pointer is untouched.
 *
 * This function returns ENOMEM on failure to allocate internal
 * structure.
 */
int globals_insert_symbol(struct globals *g, 
                          struct symbol *symbol,
                          struct symbol **existing);



/*
 * Remove all symbols in the index that isn't marked as alive.
 */
//void globals_undefine_symbols(struct globals *g);
/*
 * Remove symbol from the global symbol index.
 */
//void globals_remove_symbol(struct globals *g, const struct symbol *symbol);
/*
 * Build a string table consisting of all the symbols in the index
 * that are used.
 */
//struct strpool globals_build_string_table(struct globals *g);


#ifdef __cplusplus
}
#endif
#endif
