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
    SYMBOL_NOTYPE,  // symbol has no definition
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
    enum symbol_binding binding;    // symbol binding type
    enum symbol_type type;          // symbol type
    uint64_t align;                 // symbol address alignment requirement (value must be a multiple of align)
    uint64_t size;                  // symbol size
    bool is_absolute;               // is the definition offset relative to a section base address or an absolute address
    bool is_common;                 // does the symbol refer to a common section?
    struct section *section;        // strong reference to the section where the symbol is defined
    uint64_t offset;                // offset into the section to the definition 
};



/*
 * Helper function to determine if the symbol is defined.
 */
static inline
bool symbol_is_defined(const struct symbol *symbol)
{
    return (symbol->section != NULL || symbol->is_absolute) && !symbol->is_common;
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
 * Assign a definition to a symbol.
 *
 * If section is NULL, offset is assumed to be an absolute address.
 * Otherwise, the definition is relative to the base address of the section.
 *
 * If section is set, this function takes a strong reference.
 *
 * Note that if the symbol is already linked with a definition,
 *
 * If the symbol is weak, the previous definition is released 
 * and replaced with the new reference.
 *
 * If the symbol is strong, this function does nothing and returns
 * EALREADY.
 */
int symbol_bind_definition(struct symbol *symbol,
                           struct section *section,
                           uint64_t offset,
                           uint64_t size);
                           

/*
 * Update an existing symbol definition.
 *
 * If the incoming symbol is undefined, and the existing symbol is
 * undefined, this function does nothing and returns 0.
 *
 * If the incoming symbol is defined and existing is undefined,
 * the existing symbol is updated and 0 is returned.
 *
 * If the incoming symbol is undefined and existing is defined,
 * this function does nothing and returns 0.
 *
 * In the case where both symbols are defined, the following rules apply:
 *
 * If the incoming symbol is weak, this function does nothing
 * and returns 0 (keeps existing definition).
 * 
 * If the existing symbol is weak, the existing symbol is updated
   and this function returns 0.
 *
 * If both are strong, this function returns EEXIST.
 */
int symbol_resolve_definition(struct symbol *existing, const struct symbol *incoming);


#ifdef __cplusplus
}
#endif
#endif
