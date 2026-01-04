#ifndef _BFLD_SECTIONS_H
#define _BFLD_SECTIONS_H
#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>


// Forward declaration of section
struct section;


/* 
 * Section table.
 * Manages sections by tracking them by index.
 */
struct sections
{
    char *name;                 // section table name (for debugging)
    int refcnt;                 // reference counter
    size_t capacity;            // size of the table/array
    size_t nsections;           // number of sections in the array
    uint64_t maxidx;            // highest index inserted in the table
    struct section **entries;   // table/array of sections by index
};


/*
 * Allocate a section table.
 */
struct sections * sections_alloc(const char *name);


/*
 * Take a section table reference.
 */
struct sections * sections_get(struct sections *sections);


/*
 * Release section table reference.
 */
void sections_put(struct sections *sections);


/*
 * Reserve space for at least n sections in a section table.
 *
 * Returns true if the section table is able to hold the requested 
 * number of sections.
 *
 * Returns false if the requested number of sections is too large.
 */
bool sections_reserve(struct sections *sections, size_t n);


/*
 * Insert a section in the section table at the specified index.
 *
 * If the specified index is free, a strong reference to the section
 * is taken, the section is inserted at the specified index, and
 * 0 is returned.
 *
 * Returns EEXIST if there already is a section at the specified index. 
 *
 * If the optional existing pointer is not NULL and there already is a section
 * at the specified index, the pointer is set to the existing section. 
 * Otherwise the existing pointer is untouched.
 *
 * Returns ENOMEM if there is not enough space to insert the section.
 */
int sections_insert(struct sections *sections, 
                    uint64_t index, 
                    struct section *section,
                    struct section **existing);


/*
 * Look up section in a section table by index.
 * Note that this does not take an additional section reference.
 */
static inline
struct section * sections_at(const struct sections *sections, uint64_t index)
{
    if (index < sections->capacity) {
        return sections->entries[index];
    }
    return NULL;
}


#ifdef __cplusplus
}
#endif
#endif
