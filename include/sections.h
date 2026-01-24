#ifndef BFLD_SECTIONS_H
#define BFLD_SECTIONS_H
#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "section.h"
#include "utils/deque.h"
#include "utils/table.h"


/* 
 * Sections worklist.
 * Used to process sections in order.
 * Must be initialized to zero before use.
 */
struct sections
{
    struct deque q;     // internal queue structure
};


/*
 * Local sections table.
 * Used for looking up sections based on index.
 * Must be initialized to zero before use.
 */
struct section_table
{
    struct table tbl;   // internal table structure
    uint64_t capacity;  // the size of the table
    uint64_t nsections; // number of sections in the table
};


/*
 * Reserve space for at least n sections in the section queue.
 *
 * Returns true if the section queue is able to hold the requested 
 * number of sections.
 *
 * Returns false if the requested number of sections is too large.
 */
static inline
bool sections_reserve(struct sections *sectq, size_t n)
{
    return deque_reserve(&sectq->q, n);
}


/*
 * Insert a section at the back of the section queue.
 * Note that this takes a strong reference to the section.
 * Returns true if the section was inserted, or false if insertion failed.
 */
static inline
bool sections_push(struct sections *sectq, struct section *sect)
{
    if (!deque_push_back(&sectq->q, section_get(sect))) {
        section_put(sect);
        return false;
    }
    return true;
}


/*
 * Peek at the section at the given position relative 
 * to the start of the queue.
 */
static inline
struct section * sections_peek(const struct sections *sectq, uint64_t position)
{
    return (struct section*) deque_peek(&sectq->q, position);
}


/*
 * Remove the first section in the section queue and return it.
 * Note that this does NOT release the section reference.
 * The caller must call section_put() on the returned section reference.
 * Returns NULL if the queue is empty.
 */
static inline
struct section * sections_pop(struct sections *sectq)
{
    return (struct section*) deque_pop_front(&sectq->q);
}


/*
 * Clear the section queue.
 * Releases all sections currently in the queue.
 */
static inline
void sections_clear(struct sections *sectq)
{
    struct section *sect;

    while ((sect = sections_pop(sectq)) != NULL) {
        section_put(sect);
    }
    deque_clear(&sectq->q);
}


/*
 * Is the section queue empty?
 */
static inline
bool sections_empty(const struct sections *sectq)
{
    return sections_peek(sectq, 0) != NULL;
}


/*
 * Reserve space for at least n sections in the section table.
 *
 * Returns true if the section table is able to hold the requested
 * number of sections.
 *
 * Returns false if the requested number of sections is too large.
 */
static inline
bool section_table_reserve(struct section_table *secttab, uint64_t n)
{
    return table_reserve(&secttab->tbl, n);
}


/*
 * Get the section at the specified index.
 */
static inline
struct section * section_table_at(const struct section_table *secttab, uint64_t idx)
{
    return (struct section*) table_at(&secttab->tbl, idx);
}


/*
 * Insert a section at the specified index.
 *
 * Returns true if the section was inserted at the specified index.
 *
 * Returns false if insertion failed, either because
 * there is no space or if the specified index already
 * holds a section.
 *
 * If the existing pointer is not-NULL, existing it is
 * set to point to the existing entry.
 *
 * Note that this takes a section reference on successful insertion.
 */
static inline
bool section_table_insert(struct section_table *secttab, uint64_t idx,
                          struct section *sect, struct section **existing)
{
    if (!table_insert(&secttab->tbl, idx, (void*) section_get(sect), (void**) existing)) {
        section_put(sect);
        return false;
    }
    secttab->nsections++;
    secttab->capacity = secttab->tbl.capacity;
    return true;
}


/*
 * Remove the section at the specified index.
 * Note that this releases the section reference.
 */
static inline
void section_table_remove(struct section_table *secttab, uint64_t idx)
{
    struct section *sect = table_remove(&secttab->tbl, idx);

    if (sect != NULL) {
        section_put(sect);
        secttab->nsections--;
    }
}


/*
 * Clear the section table and release all sections.
 */
static inline
void section_table_clear(struct section_table *secttab)
{
    uint64_t idx;

    for (idx = 0; secttab->nsections > 0 && idx < secttab->tbl.capacity; ++idx) {
        section_table_remove(secttab, idx);
    }

    table_clear(&secttab->tbl);
    secttab->capacity = 0;
}


#ifdef __cplusplus
}
#endif
#endif
