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
#include "sections.h"
#include "symbols.h"


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
    struct symbols symbols;         // symbol table
    struct list_head groups;        // list of section groups
};


/*
 * A group of sections with the same type.
 */
struct section_group
{
    char *name;
    struct list_head list_entry;    // linked list entry
    struct image *image;            // weak reference to the output image
    enum section_type type;         // type of this section group
    uint64_t vaddr;                 // base virtual address for sections in the group
    uint64_t size;                  // total memory size of the section
    uint64_t align;                 // alignment requirements
    struct sections sections;       // input sections that belong to this group
};


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



bool image_reserve_capacity(struct image *image, enum section_type type, size_t nsections);


/*
 * Add a section to the image.
 * This takes a section reference.
 */
bool image_add_section(struct image *image, struct section *section);


/*
 * Calculate section's addresses and offsets based on the given base address.
 */
void image_pack(struct image *image, uint64_t base_address);


// TODO: int section_apply_relocs(...)


#ifdef __cplusplus
}
#endif
#endif
