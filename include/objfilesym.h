#ifndef __BFLD_OBJECT_SYMBOL_H__
#define __BFLD_OBJECT_SYMBOL_H__
#ifdef __cplusplus
extern "C" {
#endif


#include "symtypes.h"
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>


/*
 * Representation of a symbol detected in the object file.
 */
struct objfile_symbol
{
    const char          *name;      // symbol name
    uint64_t            value;      // value of the symbol
    enum symbol_binding binding;    // symbol binding
    enum symbol_type    type;       // symbol type
    bool                defined;    // is the symbol defined?
    uint64_t            sect_idx;   // section index (if defined)
    size_t              size;       // size of the defined symbol
    size_t              offset;     // offset within the section (if defined)
};


#ifdef __cplusplus
}
#endif
#endif
