#ifndef BFLD_SYMBOL_H
#define BFLD_SYMBOL_H
#ifdef __cplusplus
extern "C" {
#endif


#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "section.h"


/* Forward declarations */
struct linkerctx;
struct section;
struct strpool;


/*
 * Symbol types.
 */
enum symbol_type
{
    SYMBOL_NOTYPE,  // symbol has no definition
    SYMBOL_OBJECT,  // symbol is a data object, for example a global variable
    SYMBOL_TLS,     // symbol contains a thread-local data object (thread-local storage)
    SYMBOL_SECTION, // symbol is a reference to a section
    SYMBOL_FUNCTION,// symbol is a function
    SYMBOL_DEBUG,   // symbol is a debug symbol
    SYMBOL_MAX_TYPES
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
 * Symbol visibility.
 */
enum symbol_export
{
    SYMBOL_PUBLIC = 0,      // symbol is public (default for all global symbols, can be overridden)
    SYMBOL_INTERNAL = 1,    // symbol is internal
    SYMBOL_PROTECTED = 2,   // symbol is protected (visible outside unit, but can not be overridden)
    SYMBOL_PRIVATE = 3      // symbol is hidden (stripped from global symbol table)
};


/* 
 * Symbol descriptor.
 */
struct symbol
{
    int refcnt;                     // reference counter
    uint32_t hash;                  // precalculated hash of the symbol name
    struct strpool *strings;        // global string pool reference
    const char *name;               // symbol name
    enum symbol_binding binding;    // symbol binding type
    enum symbol_type type;          // symbol type
    uint64_t align;                 // symbol address alignment requirement (finalized address must be a multiple of align)
    uint64_t size;                  // symbol size
    bool is_absolute;               // is the definition offset relative to a section base address or an absolute address
    bool is_common;                 // does the symbol refer to a common section?
    enum symbol_export visibility;  // symbol visibility
    struct section *section;        // strong reference to the section where the symbol is defined
    uint64_t offset;                // offset into the section to the definition or absolute address
};


/*
 * Helper function to determine if the symbol is defined.
 */
static inline
bool symbol_is_defined(const struct symbol *symbol)
{
    return (symbol->section != NULL || symbol->is_absolute);
}


/*
 * Helper function to determine if a symbol is "alive".
 */
static inline
bool symbol_is_alive(const struct symbol *symbol)
{
    if (symbol->is_absolute) {
        return true;
    }

    if (symbol->section != NULL && symbol->section->is_alive) {
        return true;
    }

    return false;
}


/*
 * Allocate a symbol descriptor.
 */
struct symbol * symbol_alloc(const struct linkerctx *ctx,
                             const char *name,
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
 * Undefine a symbol.
 */
void symbol_undefine(struct symbol *symbol);



/*
 * Define symbol as belonging to the common section.
 */
bool symbol_define_common(struct symbol *symbol, uint64_t size, uint64_t align);
                        

/*
 * Define a symbol.
 *
 * If section is NULL, offset is assumed to be an absolute address.
 * Otherwise, the definition is relative to the base address of the section.
 *
 * If section is set, this function takes a weak reference to section.
 *
 * Note that if the symbol is already bound to a definition,
 * the behavior depends on the following:
 *
 * If the symbol is strong, the function does nothing and
 * returns false.
 *
 * If the symbol is weak, the previous definition is replaced 
 * with the new reference.
 *
 * If the symbol is common and common is set, size and align
 * is set to the largest of the current and the new definition.
 *
 * Returns true on success and false otherwise.
 */
bool symbol_define(struct symbol *symbol,
                   struct section *section,
                   uint64_t offset,
                   uint64_t size);


/*
 * Define an absolute symbol.
 */
static inline
bool symbol_define_absolute(struct symbol *symbol,
                            uint64_t address,
                            uint64_t size)
{
    return symbol_define(symbol, NULL, address, size);
}

/*
 * Update an existing symbol definition.
 *
 * If the incoming symbol is undefined, and the existing symbol is
 * undefined, this function does nothing and returns true.
 *
 * If the incoming symbol is defined and existing is undefined,
 * the existing symbol is updated and true is returned.
 *
 * If the incoming symbol is undefined and existing is defined,
 * this function does nothing and returns true.
 *
 * In the case where both symbols are defined, the following rules apply:
 *
 * If the incoming symbol is weak, this function does nothing
 * and returns true (keeps existing definition).
 * 
 * If the existing symbol is weak, the existing symbol is updated
   and this function returns true.
 *
 * If both are strong, this function returns false.
 */
bool symbol_merge(struct symbol *existing, const struct symbol *incoming);


#ifdef __cplusplus
}
#endif
#endif
