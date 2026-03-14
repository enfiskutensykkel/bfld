#ifndef BFLD_GLOBALS_H
#define BFLD_GLOBALS_H
#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
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
    uint32_t hash;          // hash of the symbol name
    uint64_t dfi;           // distance from ideal in the name hash table
    struct symbol *symbol;  // strong reference to the symbol
};


/*
 * Global symbol index.
 * Tracks global symbols by names and by sections.
 */
struct globals
{
    struct global *table;       // hash table of global symbols ordered by names
    uint64_t capacity;          // capacity of the hash table
    uint64_t nglobals;          // number of global symbols in the hash table
    uint64_t rehash_threshold;  // rehashing threshold
};


/*
 * Clear the symbol index and remove all entries.
 */
void globals_clear(struct globals *g);


/*
 * Look up a symbol from its name in the global symbol index.
 */
static inline
struct symbol * globals_find_symbol(const struct globals *g, const char *name)
{
    if (g->nglobals == 0) {
        return NULL;
    }

    uint32_t hash = hash_fnv1a_32(name, strlen(name));
    if (hash == 0) {
        hash = 1;
    }

    uint64_t slot = hash & (g->capacity - 1);
    uint64_t dfi = 0;

    const struct global *this = &g->table[slot];

    while (this->hash != 0 && dfi <= this->dfi) {
        if (this->hash == hash) {
            if (strcmp(name, symbol_name(this->symbol)) == 0) {
                return this->symbol;
            }
        }

        slot = (slot + 1) & (g->capacity - 1);
        this = &g->table[slot];
        ++dfi;
    }

    return NULL;
}


/*
 * Remove symbol from the global symbol index.
 */
void globals_remove_symbol(struct globals *g, const struct symbol *symbol);


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
 * Remove all symbols in the index that isn't marked as used.
 */
void globals_prune_dead_symbols(struct globals *g);


/*
 * Build a string table consisting of all the symbols in the index.
 */
//struct strpool globals_build_string_table(struct globals *g);


#ifdef __cplusplus
}
#endif
#endif
