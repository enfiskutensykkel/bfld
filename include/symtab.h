#ifndef __BFLD_SYMBOL_TABLE_H__
#define __BFLD_SYMBOL_TABLE_H__
#ifdef __cplusplus
extern "C" {
#endif

#include "symtypes.h"
#include "utils/list.h"
#include "utils/rbtree.h"
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>


/*
 * Forward declaration of object file.
 */
struct objfile;


/*
 * Symbol table representation.
 */
struct symtab
{
    int refcnt;             // reference counter
    char *name;             // name (used for debugging purposes)
    struct rb_tree tree;    // the tree where symbols are inserted
};


/*
 * Representation of a defined symbol.
 */
struct symbol
{
    char *name;             // symbol name
    struct symtab *table;   // symbol table this symbol is inserted into (can be NULL)
    struct rb_node tree_node;

    bool weak;              // is this a weak symbol?
    enum symbol_type type;  // symbol type
    uint64_t addr;          // absolute for an executable, relative (to base address) for a shared library

    struct objfile *definer;  // object file where the symbol is defined
    uint64_t sect_idx;      // Section index
    size_t offset;          // offset into the section to definition
    size_t size;            // size of the symbol

    struct list_head refs;  // list of files that reference this symbol
};


#define symbol_is_resolved(sym_ptr) ((sym_ptr)->definer != NULL)


/*
 * Represents a symbol reference.
 * Track which files reference a symbol.
 *
 * All symbols have at least one reference, 
 * which is where the symbol was defined.
 */
struct symref
{
    struct symbol *symbol;      // pointer to the symbol that is referenced
    struct list_head list_node;
    struct objfile *referer;
    // FIXME: more information?
};


/*
 * Convenience function to get the first refererer to a symbol.
 */
static inline
const struct objfile * symbol_referer(const struct symbol *sym)
{
    return list_first_entry(&sym->refs, struct symref, list_node)->referer;
}


/*
 * Initialize a symbol table handle.
 */
int symtab_init(struct symtab **symtab, const char *name);


/*
 * Take a symbol table reference (increase reference counter).
 */
void symtab_get(struct symtab *symtab);


/*
 * Release symbol table reference (decrease reference counter).
 *
 * If the reference count reaches zero, any remaining symbols
 * in the symbol table is also freed.
 */
void symtab_put(struct symtab *symtab);


/*
 * Look up a symbol in a given symbol table.
 */
struct symbol * symtab_find_symbol(const struct symtab *symtab, const char *name);



/*
 * Try to insert symbol in the given symbol table.
 *
 * If the symbol's name is unique in the table, the symbol is inserted
 * and 0 is returned.
 *
 * If a symbol with the same name already exists in the symbol table,
 * this function returns EEXIST. If the existing pointer is not NULL,
 * the pointer is set to the existing symbol.
 */
int symtab_insert_symbol(struct symtab *symtab, struct symbol *sym, 
                         struct symbol **existing);


/*
 * Remove a symbol from the symbol table.
 *
 * Note that this does not free the symbol; the symbol is only
 * taken out of the specified symbol table.
 */
int symtab_remove_symbol(struct symtab *symtab, struct symbol *sym);


/*
 * Replace a symbol in the given symbol table with a new symbol.
 *
 * Removes the victim from the symbol table, and replaces it with
 * the replacement.
 *
 * The caller is responsible for checking if a replacement is valid,
 * i.e., not replacing a strong symbol with a weak etc.
 *
 * Returns EINVAL if:
 * - victim's and replacement's names differ
 * - victim is not inserted into the given symbol table
 * - replacement is already inserted into a different symbol table
 *   (in which case it should be removed first)
 */
int symtab_replace_symbol(struct symtab *symtab, struct symbol *victim,
                          struct symbol *replacement);



/*
 * Allocate a symbol definition.
 */
int symbol_alloc(struct symbol **sym, struct objfile *referer, 
                 const char *name, bool weak);


/*
 * Free a symbol definition.
 * If the symbol is insterted into a symbol table,
 * it is also removed from the table first.
 */
void symbol_free(struct symbol *sym);


/*
 * Resolve a symbol definition by pointing it to where the definition is found.
 * Note that this also takes an object file reference.
 *
 * Returns EINVAL if the symbol definition was already resolved.
 */
int symbol_resolve(struct symbol *sym, struct objfile *objfile,
                   uint64_t sect_idx, size_t offset, 
                   enum symbol_type type, size_t size);
                              

/*
 * Add the specified file to the list of where the symbol is referenced.
 */
struct symref * symbol_add_reference(struct symbol *sym, struct objfile *file);


#ifdef __cplusplus
}
#endif
#endif
