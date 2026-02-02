#include "layout.h"
#include "logging.h"
#include "section.h"
#include "sections.h"
#include "sectiontype.h"
#include <assert.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "utils/align.h"


struct layout * layout_alloc(const char *name, enum section_type type, uint32_t rank)
{
    if (rank == 0) {
        rank = section_type_to_rank(type);
    }

    struct layout *l = malloc(sizeof(struct layout));
    if (l == NULL) {
        return NULL;
    }

    l->name = malloc(strlen(name) + 1);
    if (l->name == NULL) {
        free(l);
        return NULL;
    }
    strcpy(l->name, name);

    l->refcnt = 1;
    l->rank = rank;
    l->type = type;
    l->size = 0;
    l->align = 0;
    l->base_addr = 0;
    l->nsections = 0;
    l->sections = NULL;
    return l;
}


struct layout * layout_get(struct layout *l)
{
    assert(l != NULL);
    assert(l->refcnt > 0);
    l->refcnt++;
    return l;
}


void layout_put(struct layout *l)
{
    assert(l != NULL);
    assert(l->refcnt > 0);

    if (--(l->refcnt) == 0) {
        layout_clear_sections(l, NULL);
        free(l->name);
        free(l);
    }
}


static inline
bool ensure_buckets(struct layout *l, uint8_t nbuckets)
{
    uint64_t align = 1ULL << nbuckets;

    if (align <= l->align && l->sections != NULL) {
        return true;
    }

    struct sections *buckets = realloc(l->sections, sizeof(struct sections) * (nbuckets + 1));
    if (buckets == NULL) {
        return false;
    }

    uint8_t start = l->align > 0 ? align_ceillog2(l->align) + 1 : 0;
    memset(&buckets[start], 0, sizeof(struct sections) * (nbuckets + 1 - start));
    l->sections = buckets;
    l->align = align;
    return true;
}


void layout_create_worklist(const struct layout *l, struct sections *wl)
{
    if (l->sections == NULL || l->nsections == 0) {
        return;
    }

    for (uint8_t i = align_ceillog2(l->align) + 1; i > 0; --i) {
        const struct sections *bucket = &l->sections[i - 1];

        for (uint64_t idx = 0, n = sections_size(bucket); idx < n; ++idx) {
            sections_push(wl, sections_peek(bucket, idx));
        }
    }
}


void layout_clear_sections(struct layout *l, struct sections *wl)
{
    if (l->sections != NULL) {
        for (uint8_t i = align_ceillog2(l->align) + 1; i > 0; --i) {
            struct sections *bucket = &l->sections[i - 1];

            if (wl != NULL) {
                struct section *sect;

                while ((sect = sections_pop(bucket)) != NULL) {
                    sections_push(wl, sect);
                    section_put(sect);
                    l->nsections--;
                }
            } else {
                l->nsections -= sections_size(bucket);
            }
            sections_clear(bucket);
        }
        free(l->sections);
        l->sections = NULL;
    }

    l->nsections = 0;
    l->align = 0;
    l->size = 0;
    l->base_addr = 0;
}


bool layout_add_section(struct layout *l, struct section *sect)
{
    uint8_t bucket = align_ceillog2(sect->align);

    if (!ensure_buckets(l, bucket)) {
        return false;
    }

    if (sections_push(&l->sections[bucket], sect)) {
        l->size += sect->size;
        return true;
    }

    return false;
}
