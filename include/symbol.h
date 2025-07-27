#ifndef __BFLD_SYMBOL_H__
#define __BFLD_SYMBOL_H__
#ifdef __cplusplus
extern "C" {
#endif

#include <symtypes.h>
#include <utils/rbtree.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>


/*
 * Forward declaration of object file.
 */
struct objfile;


/*
 * Intermediate representation of a symbol declaration.
 */
struct symbol
{
    const char *name;
    struct rb_node tree_node;
    enum symbol_binding binding;
    enum symbol_type type;  // Defaults to SYMBOL_NOTYPE

    uint64_t addr;          // absolute for an executable, relative (to base address) for a shared library

    struct objfile *source; // object file this symbol came from
    
    bool defined;           // Is the symbol defined?
    struct objfile *definition; // object file where the symbol definition comes from
    uint64_t sect_idx;      // Section index
    size_t offset;          // offset into the section to definition
    size_t size;            // size of the symbol
};


/*
 * Look up a symbol in a symbol table with the given name.
 */
struct symbol * symbol_find(const struct rb_tree *symtab, 
                            const char *name);



/*
 * Resolve a symbol definition by pointing it to where the definition is found.
 * Note that this also takes an object file reference.
 *
 * Returns 0 if the symbol definition was successfully resolved.
 * Returns EEXIST if the symbol definition was already resolved.
 * Returns EINVAL if the specified offset and size is illegal.
 */
int symbol_resolve_definition(struct symbol *sym, struct objfile *objfile,
                              uint64_t sect_idx, size_t offset, size_t size);
                              


/*
 * Helper function to create a symbol and insert it into a symbol table.
 *
 * Returns 0 and sets the symbol pointer to the newly allocated symbol,
 * if the symbol with the given name was created and inserted.
 * Insertion occurs if either the symbol does not already exist in the
 * table, or if a symbol exists but it has a weak binding (prefer last
 * strategy, if both the incoming and the existing are both weak,
 * the last is kept).
 *
 * Returns EEXIST if a symbol with the given name already exists in
 * the symbol table and has a strong binding, and sets the symbol 
 * pointer to point to the existing symbol.
 *
 * On other failures, an errno is returned and the symbol pointer is
 * set to NULL.
 */
int symbol_create(struct symbol **symbol, 
                  struct objfile *source,
                  const char *name,
                  enum symbol_binding binding, 
                  enum symbol_type type,
                  struct rb_tree *symtab);


/*
 * Helper function to remove a symbol from the symbol table and release it.
 */
void symbol_remove(struct rb_tree *symtab, struct symbol **sym);


#ifdef __cplusplus
}
#endif
#endif
