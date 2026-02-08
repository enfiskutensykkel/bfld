#ifndef BFLD_SECTION_H
#define BFLD_SECTION_H
#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "utils/list.h"
#include "sectiontype.h"


/* Forward declarations */
struct objectfile;
struct symbol;
struct group;
struct layout;


/* 
 * Section descriptor.
 * 
 * Contains information about a section, e.g., BSS, DATA, RODATA, TEXT, etc.,
 * and relocations that need to be applied/patched.
 *
 * Note: Sections have relocations, each holding a strong reference 
 *       to its target symbol. The symbol may hold a strong reference
 *       to (another) section. To break the circular dependency,
 *       the user must call section_clear_relocs() before releasing
 *       the last section reference.
 */
struct section
{
    struct objectfile *objfile;     // strong reference to the object file the section is defined in (NOTE: can be NULL if section is synthetic)
    char *name;                     // name of the section (NOTE: can be NULL)
    int refcnt;                     // reference counter
    enum section_type type;         // section type 
    uint64_t align;                 // section alignment requirements
    uint64_t size;                  // memory size of the section
    const uint8_t *content;         // pointer to section content
    size_t nrelocs;                 // number of entries in the relocation list.
    struct list_head relocs;        // list of relocations
    //bool is_alive;                  // FIXME used for dead-code elimination / mark-and-sweep
    bool discard;                 // FIXME
    struct group *group;            // weak reference to the section group this section belongs to (if any)
    struct layout *layout;          // weak pointer to the layout (output section) this section belongs to
    uint64_t offset;                // finalized section offset from the base output section address
    struct symbol **symbols;        // dynamic array of weak references to symbols that are defined in this section
    size_t nsymbols;                // number of symbols that are defined in this section
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


static inline
const char * section_name(const struct section *section)
{
    return section->name;
}


/*
 * Allocate a section descriptor.
 * This will take a strong reference to the object file descriptor.
 */
struct section * section_alloc(struct objectfile *objectfile,
                               const char *name,
                               enum section_type type,
                               const uint8_t *content,
                               uint64_t size);


/*
 * Take a strong section reference.
 */
struct section * section_get(struct section *section);


/*
 * Release section reference.
 */
void section_put(struct section *section);


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
 * Duplicate a section and its relocations.
 *
 * Creates a new section that points to the same content
 * as the original section.
 */
struct section * section_clone(const struct section *section, const char *name);


void section_mark_alive(struct section *section);


/*
 * Add a reverse reference to a symbol defined in this section.
 */
bool section_add_symbol_reference(struct section *section, struct symbol *symbol);


/*
 * Remove a reverse reference to a symbol.
 */
void section_remove_symbol_reference(struct section *section, const struct symbol *symbol);


#ifdef __cplusplus
}
#endif
#endif
