#ifndef BFLD_MERGE_H
#define BFLD_MERGE_H
#ifdef __cplusplus
extern "C" {
#endif

#include "utils/rbtree.h"
#include "utils/list.h"
#include "sectiontype.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>


/*
 * 
 */
struct merge
{
    char *name;
    int refcnt;                     // reference count
    enum section_type type;         // type of the merged section
    uint64_t size;                  // total size of the merged section
    struct list_head sections;      // list of input sections that go into this section (ordered by alignment)
};



struct merged_section
{
    struct merge *merge;            // weak reference to the merge
    struct list_head list_entry;    // linked list entry
    struct section *section;        // strong reference to the input section
    uint64_t size;                  // size of the section (same as section->size)
    uint64_t padding;               // padding to the next section
};


struct merge_map
{
    struct rbtree map;              
};


struct merge_map_entry
{
};


bool merge_map_add_mapping(struct merge_map *map, struct merged_section* merged);


#ifdef __cplusplus
}
#endif
#endif
