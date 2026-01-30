#ifndef BFLD_BUCKET_QUEUE_H
#define BFLD_BUCKET_QUEUE_H
#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "utils/align.h"
#include "sections.h"
#include "section.h"


/*
 * Helper structure to sort sections based on alignment.
 */
struct bucket_queue
{
    int refcnt;                 // reference count
    uint64_t maxalign;          // maximum memory alignment requirements
    struct sections *buckets;   // "buckets" of sections where each alignment requirement is its own bucket
};


struct bucket_queue * bucket_queue_alloc(uint64_t maxalign);


struct bucket_queue * bucket_queue_get(struct bucket_queue *bq);


void bucket_queue_put(struct bucket_queue *bq);


/*
 * Reserve a number of buckets up to (and including) 
 * a maximum alignment given by maxalign.
 */
bool bucket_queue_reserve(struct bucket_queue *bq, uint64_t maxalign);


/*
 * Release all added sections.
 */
void bucket_queue_clear(struct bucket_queue *bq);


/*
 * Add input section to one of the buckets.
 * Note that this takes a section reference.
 */
static inline
bool bucket_queue_push(struct bucket_queue *bq, struct section *section)
{
    if (bq->buckets == NULL || section->align > bq->maxalign) {
        if (!bucket_queue_reserve(bq, section->align)) {
            return false;
        }
    }

    return sections_push(bq->buckets[align_log2(section->align)], section);
}


static inline
struct section * bucket_queue_pop(struct bucket_queue *bq)
{
    if (bq->buckets != NULL) {
        for (uint8_t i = align_log2(bq->maxalign) + 1; i != 0; --i) {
            struct sections *bucket = &bq->buckets[i - 1];
            if (!sections_empty(bucket)) {
                return sections_pop(bucket);
            }
        }
    }

    return NULL;
}


static inline
void bucket_queue_pop_all(struct bucket_queue *bq, struct sections *worklist)
{
    if (bq->sections != NULL) {
        for (uint8_t i = align_log2(bq->maxalign) + 1; i != 0; --i) {
            struct sections *bucket = &bq->buckets[i - 1];
            struct section *section;

            while ((section = sections_pop(bucket)) != NULL) {
                sections_push(worklist, section);
                section_put(section);
            }
        }
    }
}


#ifdef __cplusplus
}
#endif
#endif
