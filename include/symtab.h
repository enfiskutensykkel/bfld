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


/* Forward declaration of object file. */
struct objfile;


/* Forward declaration of a section from an object file. */
struct section;


/* Forward declaration of a merged section and a section mapping. */
struct merged_section;
struct section_mapping;


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
 * Representation of a symbol.
 */
struct symbol
{
    char *name;                 // symbol name
    struct symtab *table;       // symbol table this symbol is inserted into
    struct rb_node tree_node;   // tree node for symbol table
    bool weak;                  // is this a weak symbol that can be later replaced with a strong definition
    enum symbol_type type;      // symbol type
    bool relative;              // is the offset relative to the base address or an absolute address
    uint64_t addr;              // finalized address of the symbol
    uint64_t align;             // symbol address alignment requirement (addr must be a multiple of align)
    struct objfile *objfile;    // object file reference if symbol is defined or NULL if there is no definition
    struct section *section;    // section where the symbol is defined or NULL if there is no definition
    uint64_t offset;            // offset into the section to definition
};


static inline
bool symbol_is_undefined(const struct symbol *sym)
{
    return (sym->section == NULL && sym->relative)
        || (!sym->relative && sym->offset == 0);
}


///*
// * Represents a symbol dependency.
// * Track which files reference a symbol.
// * All symbols have at least one reference.
// *
// * FIXME: we should create these from relocations
// */
//struct symref
//{
//    struct symbol *symbol;      // pointer to the symbol that is referenced
//    struct list_head list_node;
//    struct objfile *referer;
//    // FIXME: more information?
//};


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
int symbol_alloc(struct symbol **sym, const char *name, 
                 bool weak, bool relative);


/*
 * Free a symbol definition.
 * If the symbol is insterted into a symbol table,
 * it is also removed from the table first.
 */
void symbol_free(struct symbol *sym);


/*
 * Link a symbol to its definition.
 * Returns 0 if the symbol is linked to its definition,
 * EINVAL if the symbol is absolute, and EALREADY if the
 * symbol definition was already linked.
 */
int symbol_link_definition(struct symbol *sym, struct section *sect, uint64_t offset);
                              

/*
 * Look up the merged section of a symbol.
 *
 * This does not take a merged section reference (does not increase
 * the reference count).
 */
struct merged_section * symbol_lookup_merged_section(const struct symbol *sym);


/*
 * Find out which merged section the symbol is in (from its source
 * section), and resolve symbol address.
 */
int symbol_resolve_address(struct symbol *sym);


#ifdef __cplusplus
}
#endif
#endif
