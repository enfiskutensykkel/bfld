#ifndef __BFLD_SYMBOL_H__
#define __BFLD_SYMBOL_H__
#ifdef __cplusplus
extern "C" {
#endif

#include "section.h"
#include "utils/rbtree.h"
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>


enum symbol_binding
{
    SYMBOL_GLOBAL, 
    SYMBOL_LOCAL, 
    SYMBOL_WEAK
};

/*
 * Symbol type.
 */
struct symbol
{
    char *name;
    struct rb_node tree_node;
    struct section *section;    // section where symbol is defined (NULL = undefined)
    size_t offset;              // offset into section
    
    // TODO type
};

// duplicate strong definitions = error
// weak + strong, strong wins
// multiple weaks, first wins (or another strategy, later wins)



/*
 * Look up a symbol in the symbol table with the given name.
 */
struct symbol * symbol_find(const struct rb_tree *symtab, 
                            const char *name);



/*
 * Helper function to create a symbol with a given name
 * and insert it into the symbol table.
 *
 * Returns 0 if the symbol was created and no other
 * symbol with the same name was found in the symbol
 * table, and sym is set to the newly created symbol.
 *
 * Returns EEXIST if the symbol already exists in the
 * symbol table, and sym is set to the existing symbol.
 *
 * Returns ENOMEM on other errors.
 */
int symbol_create(struct symbol **sym,
                  struct rb_tree *symtab, 
                  const char *name);


/*
 * Resolve a symbol by pointing to where it is defined.
 * This takes a section reference.
 */
int symbol_resolve(struct symbol *sym, struct section *section, size_t offset);


/*
 * Helper function to remove a symbol from the symbol table
 * and release it.
 */
void symbol_remove(struct rb_tree *symtab, struct symbol **sym);


#ifdef __cplusplus
}
#endif
#endif
