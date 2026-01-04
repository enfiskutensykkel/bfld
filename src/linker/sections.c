#include "utils/align.h"
#include "utils/rbtree.h"
#include "sections.h"
#include "section.h"
#include "logging.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>


struct sections * sections_alloc(const char *name)
{
    struct sections *st = malloc(sizeof(struct sections));
    if (st == NULL) {
        return NULL;
    }

    st->name = strdup(name);
    if (st->name == NULL) {
        free(st);
        return NULL;
    }

    st->capacity = 0; 
    st->entries = NULL;
    st->nsections = 0;
    st->refcnt = 1;
    return st;
}


bool sections_reserve(struct sections *st, size_t n)
{
    if (n <= st->capacity) {
        return true;
    }

    size_t new_capacity = align_pow2(n);

    // Sanity check that we're not overflowing
    if ((new_capacity * sizeof(struct section*)) < (st->capacity * sizeof(struct section*))) {
        return false;
    }

    log_ctx_push(LOG_CTX_NAME(st->name));
    log_trace("Extending section table capacity to %zu", new_capacity);
    log_ctx_pop();

    struct section **entries = realloc(st->entries, sizeof(struct section*) * new_capacity);
    if (entries == NULL) {
        return false;
    }

    // Zero out the new entries
    memset(&entries[st->capacity], 0, (new_capacity - st->capacity) * sizeof(struct section*));
    st->entries = entries;
    st->capacity = new_capacity;

    return true;
}


struct sections * sections_get(struct sections *st)
{
    assert(st != NULL);
    assert(st->refcnt > 0);
    ++(st->refcnt);
    return st;
}


void sections_put(struct sections *st)
{
    assert(st != NULL);
    assert(st->refcnt > 0);

    if (--(st->refcnt) == 0) {
        for (size_t i = 0; i < st->capacity && st->nsections > 0; ++i) {
            if (st->entries[i] != NULL) {
                section_put(st->entries[i]);
                st->entries[i] = NULL;
                --(st->nsections);
            }
        }
        free(st->entries);
        free(st->name);
        free(st);
    }
}


int sections_insert(struct sections *st, uint64_t idx, 
                    struct section *sect, struct section **existing)
{
    struct section **pos = NULL;

    log_ctx_push(LOG_CTX_NAME(st->name));

    if (idx >= st->capacity) {
        if (!sections_reserve(st, idx + 1)) {
            log_error("Section index %llu is too large", idx);
            log_ctx_pop();
            return ENOMEM;
        }
    }

    pos = &(st->entries[idx]);
    if (*pos != NULL) {
        log_error("Section table already contains a section with index %llu", idx);
        log_ctx_pop();
        if (existing != NULL) {
            *existing = *pos;
        }
        return EEXIST;
    }

    *pos = section_get(sect);
    ++(st->nsections);

    if (idx > st->maxidx) {
        st->maxidx = idx;
    }

    log_ctx_pop();
    return 0;
}
