#ifndef __BFLD_SECTION_H__
#define __BFLD_SECTION_H__
#ifdef __cplusplus
extern "C" {
#endif

#include "objfile.h"
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>


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
    int refcnt;             // Reference count
    enum section_type type; // Section type
    const void *data;       // Pointer to the definition/contents
    size_t size;            // Size of the content
    size_t alignment;

    // TODO: addr, alignment whatever is needed
    // TODO: defined symbols? or the other way around only?
    // TODO: relocations
};


/*
 * Increase section reference count.
 */
void section_get(struct section *sect);


/*
 * Decrease section reference count.
 */
void section_put(struct section *sect);


#ifdef __cplusplus
}
#endif
#endif
