#ifndef _BFLD_SECTION_H
#define _BFLD_SECTION_H
#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include "utils/list.h"


/*
 * Section types.
 */
enum section_type
{
    SECTION_ZERO,       // section without contents, i.e., uninitialized variables (.bss, .common)
    SECTION_DATA,       // section with data contents, for example variables (.data)
    SECTION_RODATA,     // section contains read-only data, for example strings (.rodata)
    SECTION_TEXT        // section contains machine code (.text)
};


/* Forward declaration of object file handle */
struct objfile;


/* 
 * Section descriptor.
 * 
 * Contains information about a section, e.g., BSS, DATA, RODATA, TEXT, etc.,
 * and relocations that need to be applied/patched.
 */
struct section
{
    struct objfile *objfile;        // strong reference to the object file the section is defined in
    char *name;                     // name of the section (NOTE: can be NULL)
    uint64_t idx;                   // section index in the object file (used for debugging)
    int refcnt;                     // reference counter
    enum section_type type;         // section type 
    uint64_t align;                 // section alignment requirements (addr must be a multiple of align)
    size_t size;                    // size of the section
    const uint8_t *content;         // pointer to section content
    size_t nrelocs;                 // number of entries in the relocation list.
    struct list_head relocs;        // list of relocations
};


/*
 * Relocation that must be applied to a section.
 *
 * A relocation is a "hole" within a section that needs to be patched
 * with a resolved address (of a symbol), or the "target".
 */
struct reloc
{
    struct list_head list_entry;    // list entry
    struct section *section;        // weak reference to the section.
    uint64_t offset;                // offset within section to where the relocation should be applied
    struct symbol *symbol;          // strong reference to the symbol the relocation refers to
    uint32_t type;                  // relocation type, which kind of "patch" to apply
    int64_t addend;                 // relocation addend
};


/*
 * Allocate a section descriptor.
 * This will take a strong reference to the object file descriptor.
 */
struct section * section_alloc(struct objfile *objfile,
                               uint64_t idx,
                               const char *name,
                               enum section_type type,
                               const uint8_t *content,
                               size_t size);


/*
 * Add a relocation to the section's relocation list.
 * This will take a strong reference to the symbol.
 */
struct reloc * section_add_reloc(struct section *section, 
                                 uint64_t offset, 
                                 struct symbol *symbol,
                                 uint32_t type,
                                 int64_t addend);


/*
 * Remove relocation from the section's relocation list,
 * and delete it. This will release the strong reference
 * to the symbol it refers to.
 */
void section_remove_reloc(struct reloc *reloc);


/*
 * Clear all relocations from the section's relocation list.
 */
void section_clear_relocs(struct section *section);



/*
 * Take a strong section reference.
 */
struct section * section_get(struct section *section);


/*
 * Release section reference.
 */
void section_put(struct section *section);



// TODO: int section_apply_relocs(...)


#ifdef __cplusplus
}
#endif
#endif
