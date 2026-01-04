#ifndef _BFLD_SYMBOL_H
#define _BFLD_SYMBOL_H
#ifdef __cplusplus
extern "C" {
#endif


#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


/*
 * Symbol types.
 */
enum symbol_type
{
    SYMBOL_NOTYPE,  // symbol has no data/definition
    SYMBOL_OBJECT,  // symbol is a data object, for example a global variable
    SYMBOL_TLS,     // symbol contains a thread-local data object (thread-local storage)
    SYMBOL_SECTION, // symbol is a reference to a section
    SYMBOL_FUNCTION // symbol is a function
};


/*
 * Symbol binding types.
 */
enum symbol_binding
{
    SYMBOL_WEAK,    // symbol is weak that can later be replaced with a strong definition
    SYMBOL_GLOBAL,  // symbol is global, exported so it is visible outside its object file
    SYMBOL_LOCAL    // symbol is local to the object file it is defined in
};


/* 
 * Symbol descriptor.
 */
struct symbol
{
    char *name;                     // symbol name
    int refcnt;                     // reference counter
    uint64_t value;                 // finalized address of the symbol (absolute or relative to section start)
    bool relative;                  // is the offset relative to the base address or an absolute address
    enum symbol_binding binding;    // symbol binding type
    enum symbol_type type;          // symbol type
    uint64_t align;                 // symbol address alignment requirement (addr must be a multiple of align)
    struct section *section;        // strong reference to the section where the symbol is defined
    uint64_t offset;                // offset into the section to the definition
};


/*
 * Helper function to determine if the symbol is defined.
 */
static inline
bool symbol_is_defined(const struct symbol *symbol)
{
    return symbol->section != NULL || (symbol->relative && symbol->offset != 0);
}


/*
 * Allocate a symbol descriptor.
 */
struct symbol * symbol_alloc(const char *name,
                             enum symbol_type type,
                             enum symbol_binding binding);


/*
 * Increase symbol descriptor's reference counter.
 */
struct symbol * symbol_get(struct symbol *symbol);


/*
 * Decrease symbol descriptor's reference counter.
 * When the reference counter becomes zero, the symbol
 * descriptor is freed. If the symbol is defined, i.e.,
 * section is not NULL, the section reference is released.
 */
void symbol_put(struct symbol *symbol);


/*
 * Link an undefined symbol to its definition.
 *
 * Takes a strong reference to the section.
 *
 * If the symbol is not undefined, i.e. it is already
 * linked with a definition, the following applies:
 *
 * If the symbol is weak, the previous definition
 * is released and replaced with the new reference.
 *
 * If the symbol is strong, the function returns EALREADY.
 */
int symbol_link_definition(struct symbol *symbol,
                           struct section *section,
                           uint64_t offset);
                           

/*
 * Update an existing symbol definition.
 *
 * If the incoming symbol is undefined, and the existing symbol is
 * undefined, this function returns 0 and does nothing.
 *
 * If the incoming symbol is defined and existing is undefined,
 * the existing symbol is updated and 0 is returned.
 *
 * If the incoming symbol is undefined and existing is defined,
 * the incoming symbol is updated and 0 is returned.
 *
 * In the case where both symbols are defined, the following rules apply:
 *
 * If the incoming symbol is weak, this function returns 0 
 * and does nothing.
 * 
 * If the existing symbol is weak, the incoming symbol is updated.
 *
 * If both are strong, this function returns ENOTUNIQ.
 */
int symbol_resolve_definition(struct symbol *existing, struct symbol *incoming);


#ifdef __cplusplus
}
#endif
#endif
