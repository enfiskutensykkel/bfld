#ifndef __BFLD_OBJECT_FILE_TYPES_H__
#define __BFLD_OBJECT_FILE_TYPES_H__
#ifdef __cplusplus
extern "C" {
#endif


#include "symtypes.h"
#include "secttypes.h"
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


/*
 * Representation of a section found in an object file.
 */
struct objfile_section
{
    const char          *name;      // name of the section
    uint64_t            sect_idx;   // section index
    enum section_type   type;       // section type (data, rodata, text, etc.)
    uint64_t            align;      // section alignment requirements
    size_t              size;       // size of the section
    size_t              offset;     // offset from file start to start of section
};


/* Forward declaration of an object file handle */
struct objfile;


#ifdef __cplusplus
}
#endif
#endif
