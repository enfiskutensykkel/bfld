#ifndef _BFLD_SECTION_TABLE_H
#define _BFLD_SECTION_TABLE_H
#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "utils/rbtree.h"


// Some forward declarations
struct section;
struct secttab_entry;


/* 
 * Section table.
 */
struct secttab
{
    char *name;                         // section table name (for debugging)
    int refcnt;                         // reference counter
    struct rb_tree name_map;            // map of sections by name
    size_t capacity;                    // table capacity
    size_t nsections;                   // number of sections
    struct section **sections;          // section table
};


/*
 * Section table entry (internal structure).
 */
struct secttab_entry
{
    struct secttab *secttab;    // weak reference to the section table
    uint64_t section_idx;       // section index
    struct section *section;    // strong reference to section
    struct rb_node map_entry;   // entry in the name-to-section map
};


/*
 * Allocate a section table.
 */
struct secttab * secttab_alloc(const char *name);



/*
 * Take a section table reference.
 */
struct secttab * secttab_get(struct secttab *table);


/*
 * Release section table reference.
 */
void secttab_put(struct secttab *table);


/*
 * Insert a section in the section table.
 * Returns true if the section was inserted,
 * or false if something went wrong.
 */
bool secttab_insert_section(struct secttab *table, 
                            uint64_t section_idx, 
                            struct section *section);
                                     

/*
 * Look up section in a section table by index.
 */
struct section * secttab_get_section(struct secttab *table,
                                     uint64_t section_idx);


/*
 * Look up section in a section table by name.
 */
struct section * secttab_get_section_by_name(struct secttab *table,
                                             const char *section_name);


#ifdef __cplusplus
}
#endif
#endif
