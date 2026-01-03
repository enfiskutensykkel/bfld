#ifndef _BFLD_SYMBOL_TABLE_H
#define _BFLD_SYMBOL_TABLE_H
#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include "utils/rbtree.h"

struct symbol;


/*
 * Symbol table.
 */
struct symtab
{
    char *name;                 // name (used for debugging purposes)
    int refcnt;                 // reference counter
    size_t nsymbols;            // number of symbols in the symbol table
    struct rb_tree map;         // map of symbols by name
};


/*
 * Entry in the symbol table (internal structure).
 */
struct symtab_entry
{
    struct rb_node map_entry;   // map entry data
    struct symtab *symtab;      // weak reference to the symbol table
    struct symbol *symbol;      // strong reference to the symbol
};


/*
 * Create a symbol table.
 */
struct symtab * symtab_alloc(const char *name);


/*
 * Take a symbol table reference.
 */
struct symtab * symtab_get(struct symtab *table);


/*
 * Release symbol table reference.
 */
void symtab_put(struct symtab *table);


/*
 * Insert symbol in the symbol table.
 * 
 * If the symbol's name is unique, a strong reference to
 * the symbol is taken, the symbol is inserted into the symbol table
 * and this function returns 0.
 *
 * If a symbol with the same name already exists in the symbol table,
 * this funtion returns EEXIST. If the optional existing pointer is 
 * non-NULL, the pointer is set to the existing symbol.
 *
 * This function returns ENOMEM on failure to allocate internal
 * structure.
 */
int symtab_insert_symbol(struct symtab *table, struct symbol *symbol,
                         struct symbol **existing);


/*
 * Look up a symbol in the symbol table.
 */
struct symbol * symtab_find_symbol(const struct symtab *table, const char *name);



#ifdef __cplusplus
}
#endif
#endif
