#include "merge.h"
#include "section.h"
#include "sections.h"
#include "sectiontype.h"
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "utils/align.h"


//
///*
// * Add input section to one of the buckets.
// * Note that this takes a section reference.
// */
//static inline
//bool bucket_queue_push(struct bucket_queue *bq, struct section *section)
//{
//    if (bq->buckets == NULL || section->align > bq->maxalign) {
//        if (!bucket_queue_reserve(bq, section->align)) {
//            return false;
//        }
//    }
//
//    return sections_push(bq->buckets[align_log2(section->align)], section);
//}
//
//
//static inline
//struct section * bucket_queue_pop(struct bucket_queue *bq)
//{
//    if (bq->buckets != NULL) {
//        for (uint8_t i = align_log2(bq->maxalign) + 1; i != 0; --i) {
//            struct sections *bucket = &bq->buckets[i - 1];
//            if (!sections_empty(bucket)) {
//                return sections_pop(bucket);
//            }
//        }
//    }
//
//    return NULL;
//}
//
//
//static inline
//void bucket_queue_pop_all(struct bucket_queue *bq, struct sections *worklist)
//{
//    if (bq->sections != NULL) {
//        for (uint8_t i = align_log2(bq->maxalign) + 1; i != 0; --i) {
//            struct sections *bucket = &bq->buckets[i - 1];
//            struct section *section;
//
//            while ((section = sections_pop(bucket)) != NULL) {
//                sections_push(worklist, section);
//                section_put(section);
//            }
//        }
//    }
//}
//
//static bool grow_sections(struct merge *merge, uint8_t buckets)
//{
//    uint8_t maxalign = 1ULL << buckets;
//
//    if (maxalign <= merge->maxalign && merge->sections != NULL) {
//        return true;
//    }
//
//    struct sections *sects = realloc(merge->sections, sizeof(struct sections) * (buckets + 1));
//    if (sects == NULL) {
//        return false;
//    }
//
//    uint8_t first_bucket = merge->maxalign > 0 ? align_pow2exp(merge->maxalign) + 1 : 0;
//    memset(&sects[first_bucket], 0, sizeof(struct sections) * (buckets + 1 - first_bucket));
//
//    merge->sections = sects;
//    merge->maxalign = maxalign;
//    return true;
//}
//
//
//struct merge * merge_alloc(const char *name, enum section_type type)
//{
//    struct merge *merge = malloc(sizeof(struct merge));
//    if (merge == NULL) {
//        return NULL;
//    }
//
//    merge->name = NULL;
//    merge->refcnt = 1;
//    merge->maxalign = 0;
//    merge->type = type;
//    merge->sections = NULL;
//
//    if (name != NULL) {
//        merge->name = strdup(name);
//        if (merge->name == NULL) {
//            free(merge);
//            return NULL;
//        }
//    }
//
//    return merge;
//}
//
//
//struct merge * merge_get(struct merge *merge)
//{
//    assert(merge != NULL);
//    assert(merge->refcnt > 0);
//    merge->refcnt++;
//    return merge;
//}
//
//
//void merge_put(struct merge *merge)
//{
//    assert(merge != NULL);
//    assert(merge->refcnt > 0);
//
//    if (--(merge->refcnt) == 0) {
//        merge_clear(merge);
//        if (merge->name != NULL) {
//            free(merge->name);
//        }
//        free(merge);
//    }
//}
//
//
//void merge_clear(struct merge *merge)
//{
//    if (merge->sections != NULL) {
//        for (uint8_t i = 0, n = align_pow2exp(merge->maxalign) + 1; i < n; ++i) {
//            sections_clear(&merge->sections[i]);
//        }
//        free(merge->sections);
//    }
//
//    merge->size = 0;
//    merge->maxalign = 0;
//    merge->sections = NULL;
//}
//
//
//bool merge_add_section(struct merge *merge, struct section *section)
//{
//    uint8_t bucket = align_pow2exp(section->align);
//
//    if (!grow_sections(merge, bucket)) {
//        return false;
//    }
//
//    if (sections_push(&merge->sections[bucket], section)) {
//        merge->size += section->size;
//        return true;
//    }
//
//    return false;
//}
//
//
//void merge_flatten(struct merge *merge, struct sections *worklist)
//{
//    if (merge->sections == NULL || merge->maxalign == 0) {
//        return;
//    }
//
//    for (int i = align_pow2exp(merge->maxalign); i >= 0; --i) {
//        const struct sections *bucket = &merge->sections[i];
//
//        for (uint64_t j = 0; j < sections_size(bucket); ++j) {
//            sections_push(worklist, sections_peek(bucket, j));
//        }
//    }
//}
