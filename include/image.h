#ifndef BFLD_IMAGE_H
#define BFLD_IMAGE_H
#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "utils/list.h"
#include "section.h"
#include "symbols.h"


// Forward declaration
struct image;


/*
 * A group of sections with the same type.
 */
struct section_group
{
    struct image *image;            // weak reference to the output image
    char *name;                     // name of the section group
    enum section_type type;         // type of this section group
    struct list_head list_entry;    // linked list entry
    uint64_t vaddr;                 // base virtual address for sections in the group
    uint64_t size;                  // total memory size of the section
    uint64_t align;                 // alignment requirements
    struct list_head sections;      // list of output sections that belong to the section group
};


/*
 * Output section.
 */
struct output_section
{
    struct section_group *group;    // weak reference to the output image
    struct list_head list_entry;    // linked list entry
    uint64_t offset;                // finalized virtual address offset
    uint64_t size;                  // section size
    struct section *section;        // reference to section content
};


/*
 * The final output image for the linker.
 */
struct image
{
    char *name;
    uint32_t target;                // target architecture
    uint64_t cpu_align;             // CPU code alignment requirements
    uint64_t min_page_size;         // minimum page size
    uint64_t max_page_size;         // maximum page size
    bool is_be;                     // is big-endian?
    uint64_t base_addr;             // base virtual address of the image
    uint64_t entrypoint;            // address of the image's entrypoint
    uint64_t size;                  // total memory size
    int refcnt;                     // reference counter
    struct list_head groups;        // section groups
    struct symbols symbols;         // symbols used by the image
};


/*
 * Create an output image.
 */
struct image * image_alloc(const char *name, 
                           uint32_t target, 
                           uint64_t cpu_align,
                           uint64_t min_page_size,
                           uint64_t max_page_size,
                           bool is_be);


/*
 * Take an image reference.
 */
struct image * image_get(struct image *image);


/*
 * Release image reference.
 */
void image_put(struct image *image);


/*
 * Add a section to the image.
 * This takes a section reference.
 */
bool image_add_section(struct image *image, struct section *section);


/*
 * Add a symbol to the image.
 * This takes a symbol reference.
 */
bool image_add_symbol(struct image *image, struct symbol *symbol);


/*
 * Calculate section's addresses and offsets based on the given base address.
 */
void image_pack(struct image *image, uint64_t base_address);


// TODO: int section_apply_relocs(...)


#ifdef __cplusplus
}
#endif
#endif
