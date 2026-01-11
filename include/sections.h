#ifndef BFLD_SECTIONS_H
#define BFLD_SECTIONS_H
#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "section.h"


// Forward declaration of section
struct section;


/* 
 * Section table.
 * Manages sections by tracking them by index.
 */
struct sections
{
    char *name;                 // section table name (NOTE: can be NULL)
    int refcnt;                 // reference counter
    size_t capacity;            // size of the table/array
    size_t nsections;           // number of sections in the array
    uint64_t maxidx;            // the highest inserted index
    struct section **sections;  // table/array of sections by index
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
 * Insert a section in the back of the section table.
 * Takes a strong reference to the section and returns the index
 * the section was inserted into.
 *
 * Note: Returns 0 if there was not enough space to insert the section.
 */
uint64_t sections_push(struct sections *sections, struct section *section);


/*
 * Release the reference at the specified index.
 */
bool sections_remove(struct sections *sections, uint64_t index);


/*
 * Helper function to "pop" the section with the highest index off the table.
 *
 * Note that this does NOT release the section reference.
 * The caller must call section_put() on the returned reference.
 */
struct section * sections_pop(struct sections *sections);


/*
 * Look up section in a section table by index.
 * Note that this does not take an additional section reference.
 */
static inline
struct section * sections_at(const struct sections *sections, uint64_t index)
{
    if (index < sections->capacity) {
        return sections->sections[index];
    }
    return NULL;
}


/*
 * Clear the sections table.
 */
void sections_clear(struct sections *sections);


#ifdef __cplusplus
}
#endif
#endif
