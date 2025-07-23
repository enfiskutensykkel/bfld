#ifndef __BFLD_SECTION_H__
#define __BFLD_SECTION_H__
#ifdef __cplusplus
extern "C" {
#endif


#include "mfile.h"


enum section_type
{
    SECTION_ZERO,   // Section without contents, i.e., unitialized variables (.bss)
    SECTION_DATA,   // Section with data contents, for example variables. (.data)
    SECTION_RODATA, // Section contains read-only data, for example strings (.rodata)
    SECTION_TEXT    // Section contains machine code
};


struct section
{
    mfile *file;            // File reference
    enum section_type type; // Section type
    const void *ptr;        // Pointer to the definition/contents
    size_t size;            // Size of the content

    // TODO: addr, alignment whatever is needed
};


#ifdef __cplusplus
}
#endif
#endif
