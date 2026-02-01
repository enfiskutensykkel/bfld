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
    SECTION_CODE,       // section contains machine code (.text, .eh_frame)
    SECTION_READONLY,   // section contains read-only data, for example strings (.rodata)
    SECTION_DATA,       // section with data contents, for example variables (.data)
    SECTION_ZERO,       // section without contents, i.e., uninitialized variables (.bss, .common)
    SECTION_METADATA,   // synthetic metadata such as symbol tables (.symtab, strtab)
    SECTION_DEBUG,      // debug information (.debug_info)
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
        case SECTION_CODE:
            return 10;
        case SECTION_READONLY:
            return 20;
        case SECTION_DATA:
            return 30;
        case SECTION_ZERO:
            return 9000;
        case SECTION_DEBUG:
        case SECTION_METADATA:
        default:
            return UINT32_MAX;
    }
}


#ifdef __cplusplus
}
#endif
#endif
