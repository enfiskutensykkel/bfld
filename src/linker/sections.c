#include "utils/align.h"
#include "utils/rbtree.h"
#include "sections.h"
#include "section.h"
#include "logging.h"
#include "symbol.h"
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

    if (name != NULL) {
        st->name = strdup(name);
    }

    st->capacity = 0; 
    st->sections = NULL;
    st->nsections = 0;
    st->maxidx = 0;
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

    struct section **sections = realloc(st->sections, sizeof(struct section*) * new_capacity);
    if (sections == NULL) {
        return false;
    }

    // Zero out the new entries
    memset(&sections[st->capacity], 0, (new_capacity - st->capacity) * sizeof(struct section*));
    st->sections = sections;
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


void sections_clear(struct sections *st)
{
    for (size_t i = 0; i < st->capacity && st->nsections > 0; ++i) {
        if (st->sections[i] != NULL) {
            section_put(st->sections[i]);
            //st->sections[i] = NULL;
            st->nsections--;
        }
    }
    st->maxidx = 0;
    st->nsections = 0;
    free(st->sections);
    st->sections = NULL;
    st->capacity = 0;
}


void sections_put(struct sections *st)
{
    assert(st != NULL);
    assert(st->refcnt > 0);

    if (--(st->refcnt) == 0) {
        sections_clear(st);
        free(st->sections);
        if (st->name != NULL) {
            free(st->name);
        }
        free(st);
    }
}


uint64_t sections_push(struct sections *st, struct section *sect)
{
    uint64_t idx;
    uint64_t maxidx = st->maxidx;

    do {
        idx = ++maxidx;

        if (idx >= st->capacity) {
            if (!sections_reserve(st, idx + 1)) {
                return 0;
            }
        }

    } while (st->sections[idx] != NULL);

    st->sections[idx] = section_get(sect);
    st->nsections++;
    st->maxidx = maxidx;
    return idx;
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

    pos = &(st->sections[idx]);
    if (*pos != NULL) {
        log_error("Section table already contains a section with index %llu", idx);
        log_ctx_pop();
        if (existing != NULL) {
            *existing = *pos;
        }
        return EEXIST;
    }

    *pos = section_get(sect);
    st->nsections++;

    if (idx > st->maxidx) {
        st->maxidx = idx;
    }

    log_ctx_pop();
    return 0;
}


bool sections_remove(struct sections *st, uint64_t idx)
{
    if (idx >= st->capacity) {
        return false;
    }

    struct section **s = &(st->sections[idx]);
    if (*s == NULL) {
        return false;
    }

    section_put(*s);
    *s = NULL;
    st->nsections--;

    if (idx == st->maxidx) {
        while (st->maxidx > 0 && st->sections[st->maxidx] == NULL) {
            st->maxidx--;
        }
    }

    return true;
}


void sections_sweep_dead(struct sections *st, bool compact)
{
    size_t n = 0;

    for (size_t i = 0; i < st->capacity; ++i) {
        struct section **s = &st->sections[i];

        if (*s == NULL) {
            continue;
        }

        if ((*s)->is_alive) {
            if (compact && i != n) {
                st->sections[n] = *s;
                *s = NULL;
            }
            ++n;
        } else {
            section_put(*s);
            *s = NULL;
        }
    }

    st->nsections = n;
}


struct section * sections_pop(struct sections *st)
{
    if (st->maxidx > 0) {
        assert(st->maxidx < st->capacity);
        assert(st->sections[st->maxidx] != NULL);

        struct section *sect = section_get(st->sections[st->maxidx]);
        sections_remove(st, st->maxidx);
        return sect;
    }
    return NULL;
}
