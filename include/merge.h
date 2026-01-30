#ifndef BFLD_MERGE_H
#define BFLD_MERGE_H
#ifdef __cplusplus
extern "C" {
#endif

#include "sectiontype.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>


// Forward declarations
struct section;
struct sections;


/*
 * Merged section handle.
 *
 * Represents a group of sections that should be grouped together
 * into a single section.
 */
struct merge
{
    char *name;                 // name of the merged section (NOTE: may be NULL)
    int refcnt;                 // reference count
    enum section_type type;     // type of the merged section
    uint64_t size;              // total memory size of the merged section
    uint64_t maxalign;          // maximum memory alignment requirements
    struct sections *sections;  // table of sections sorted on alignment
};


/*
 * Allocate a merged section handle.
 */
struct merge * merge_alloc(const char *name, 
                           enum section_type type);


/*
 * Take a merged section reference.
 */
struct merge * merge_get(struct merge *merge);


/*
 * Release a merged section reference.
 */
void merge_put(struct merge *merge);


/*
 * Add input section to the merged sections.
 * Takes a section reference.
 */
bool merge_add_section(struct merge *merge, struct section *section);


/*
 * Release all added sections.
 */
void merge_clear(struct merge *merge);


void merge_flatten(struct merge *merge, struct sections *worklist);


#ifdef __cplusplus
}
#endif
#endif
