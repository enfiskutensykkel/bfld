#ifndef _BFLD_GLOBALS_H
#define _BFLD_GLOBALS_H
#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include "utils/rbtree.h"


// Forward declaration of symbol
struct symbol;


/*
 * Global symbol table.
 * Manages symbols by tracking them by their name.
 */
struct globals
{
    char *name;                 // global symbol table name (used for debugging purposes)
    int refcnt;                 // reference counter
    size_t nsymbols;            // number of symbols in the symbol table
    struct rb_tree map;         // map of symbols by name
};


/*
 * Entry in the global symbol table (internal data structure).
 */
struct globals_entry
{
    struct globals *globals;    // weak reference to the global symbol table
    struct rb_node map_entry;   // map entry data
    struct symbol *symbol;      // strong reference to the symbol
};


/*
 * Create a global symbol table.
 */
struct globals * globals_alloc(const char *name);


/*
 * Take a global symbol table reference.
 */
struct globals * globals_get(struct globals *globals);


/*
 * Release the global symbol table reference.
 */
void globals_put(struct globals *globals);


/*
 * Insert symbol in the global symbol table.
 * 
 * If the symbol's name is unique, a strong reference to
 * the symbol is taken, the symbol is inserted into the table,
 * and the function returns 0.
 *
 * If a symbol with the same name already is inserted in the symbol 
 * table, this funtion returns EEXIST. 
 *
 * If the optional existing pointer is non-NULL and a symbol with the same
 * name already is inserted, the pointer is set to the existing symbol.
 * Otherwise the existing pointer is untouched.
 *
 * This function returns ENOMEM on failure to allocate internal
 * structure.
 */
int globals_insert_symbol(struct globals *globals,
                          struct symbol *symbol,
                          struct symbol **existing);


/*
 * Look up a symbol in the global symbol table.
 */
struct symbol * globals_find_symbol(const struct globals *globals, const char *name);



#ifdef __cplusplus
}
#endif
#endif
