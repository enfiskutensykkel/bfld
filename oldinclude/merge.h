#ifndef __BFLD_MERGE_H__
#define __BFLD_MERGE_H__
#ifdef __cplusplus
extern "C" {
#endif

#include "secttypes.h"
#include "objfile.h"
#include "utils/list.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>


/*
 * Represents a "merged" section, i.e., sections of the same type
 * merged together.
 */
struct merged_section
{
    int refcnt;                 // reference counter
    char *name;                 // section name
    enum section_type type;     // section type
    struct list_head mappings;  // section mappings
    uint64_t addr;              // base address
    uint64_t align;             // required alignment
    uint64_t total_size;        // total size of the merged section
    uint8_t *content;           // NULL until 
};


struct section_mapping
{
    struct merged_section *merged_section;  // pointer to the merged section
    struct list_head list_node;
    struct objfile *objfile;    // object file reference
    struct section *section;    // section where the data comes from
    uint64_t offset;            // offset in merged section
    const uint8_t *content;     // section content pointer
    size_t size;                // size of the section content
};


int merged_section_init(struct merged_section **merged, 
                        const char *name,
                        enum section_type type);


void merged_section_get(struct merged_section *merged);


void merged_section_put(struct merged_section *merged);


/*
 * Add section to a merged section.
 */
int merged_section_add_section(struct merged_section *merged, struct section *sect);


/*
 * Set the merged section's base address and calculate all offsets.
 */
int merged_section_finalize_addresses(struct merged_section *merged, 
                                      uint64_t base_addr);



#ifdef __cplusplus
}
#endif
#endif
