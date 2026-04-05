#ifndef BFLD_SECTION_TYPE_H
#define BFLD_SECTION_TYPE_H
#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>


/*
 * Section types.
 */
enum section_type
{
    SECTION_CODE,       // loadable section containing machine code (.text, .init)
    SECTION_UNWIND,     // loadable section containing machine code (.eh_frame)
    SECTION_THUNK,      // loadable section containing generated trampolines (.plt), same attributes as SECTION_CODE
    SECTION_READONLY,   // loadable section containing read-only data, for example constants, string literals etc. (.rodata, .eh_frame)
    SECTION_DATA,       // loadable section with data contents, for example variables (.data) or the global offset table (.got)
    SECTION_ZERO,       // loadable section without contents, i.e., uninitialized variables (.bss, .common)
    SECTION_LOADER,     // loadable read-only section with path to loader/interpreter (.interp)
    SECTION_METADATA,   // non-loadable section with metadata (.symtab, .strtab, .shstrtab)
    SECTION_DEBUG,      // non-loadable debug information (has relocations) (.debug_info, .debug_line)
    SECTION_MAX_TYPES
};


/*
 * Helper function to get the "rank" of a section type.
 * Lower is higher priority.
 */
static inline
uint32_t section_type_to_rank(enum section_type type)
{
    switch (type) {
        case SECTION_LOADER:
            return 5;
        case SECTION_CODE:
            return 10;
        case SECTION_THUNK:
            return 11;
        case SECTION_READONLY:
            return 20;
        case SECTION_DATA:
            return 30;
        case SECTION_ZERO:
            return 9000;
        case SECTION_METADATA:
        case SECTION_DEBUG:
        default:
            return UINT32_MAX;
    }
}


static inline
const char * section_type_to_string(enum section_type type)
{
    switch (type) {
        case SECTION_CODE:
            return "SECTION_CODE";
        case SECTION_UNWIND:
            return "SECTION_UNWIND";
        case SECTION_THUNK:
            return "SECTION_THUNK";
        case SECTION_READONLY:
            return "SECTION_READONLY";
        case SECTION_DATA:
            return "SECTION_DATA";
        case SECTION_ZERO:
            return "SECTION_ZERO";
        case SECTION_LOADER:
            return "SECTION_LOADER";
        case SECTION_METADATA:
            return "SECTION_METADATA";
        case SECTION_DEBUG:
            return "SECTION_DEBUG";
        default:
            return "SECTION_TYPE_INVALID";
    }
}


#ifdef __cplusplus
}
#endif
#endif
