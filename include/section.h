#ifndef __BFLD_SECTION_H__
#define __BFLD_SECTION_H__
#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>


/*
 * Forward declaration of an object file.
 * Object files have custom implementations,
 * depending on the file loader that parses it.
 */
struct objfile;


/*
 * Intermediate representation of a section with data.
 */
struct section
{
    enum {
        SECTION_ZERO,   // Section without contents, i.e., unitialized variables (.bss)
        SECTION_DATA,   // Section with data contents, for example variables. (.data)
        SECTION_RODATA, // Section contains read-only data, for example strings (.rodata)
        SECTION_TEXT    // Section contains machine code
    } type; 

    const struct objfile *objfile; // reference to the object file where the section came from
    uint64_t sect_idx;  // Section index, used for identifying the section 
    uint64_t align;     // Memory alignment
    uint64_t addr;      // Absolute or relative address of the loaded section
    uint64_t size;      // Size of the content

    const uint8_t *data;  // Pointer to the definition/contents (can be NULL)
    size_t data_size;   // Size of the contents on file (can be 0)
};


#ifdef __cplusplus
}
#endif
#endif
