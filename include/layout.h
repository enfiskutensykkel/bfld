#ifndef BFLD_LAYOUT_H
#define BFLD_LAYOUT_H
#ifdef __cplusplus
extern "C" {
#endif

#include "sectiontype.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>


/* Forward declaration of sections */
struct sections;


/*
 * Layout section.
 *
 * Groups input sections together so that they can
 * be written as a single section in the output image.
 *
 * Input sections are ordered by alignment, so that the
 * sections with the highest alignment requirement go first.
 * Sections with the same alignment are ordered by first in-first out.
 */
struct layout
{
    char *name;                 // output section name
    int refcnt;                 // reference counter
    uint32_t rank;              // order or rank of the output section
    enum section_type type;     // output section type
    uint64_t size;              // total size of the output section
    uint64_t align;             // the highest alignment requirement of all input sections
    uint64_t base_vaddr;        // base virtual address of the output section
    uint64_t nsections;         // total number of sections, used primarily for debugging
    struct sections *sections;  // dynamic array of sections ordered by alignment
};


/*
 * Allocate a layout section.
 * If rank is 0, the rank is inferred from section type.
 */
struct layout * layout_alloc(const char *name, 
                             enum section_type type, 
                             uint32_t rank);


/*
 * Take a layout reference.
 */
struct layout * layout_get(struct layout *layout);


/*
 * Release a layout reference.
 */
void layout_put(struct layout *layout);


/*
 * Add an input section to the layout.
 * Takes a section reference on successful insertion.
 */
bool layout_add_section(struct layout *layout, struct section *section);


/*
 * Traverse the sections and add them to a flattened section worklist.
 */
void layout_create_worklist(const struct layout *layout, struct sections *worklist);


/*
 * Remove all added input sections from the layout and release their reference.
 *
 * If the worklist argument is not NULL, the sections are added (in order) to
 * the the worklist.
 */
void layout_clear_sections(struct layout *layout, struct sections *worklist);



#ifdef __cplusplus
}
#endif
#endif
