#include "symbols.h"
#include "symbol.h"
#include "logging.h"
#include "utils/rbtree.h"
#include "utils/align.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>


struct symbols * symbols_alloc(const char *name)
{
    struct symbols *syms = malloc(sizeof(struct symbols));
    if (syms == NULL) {
        return NULL;
    }

    syms->name = strdup(name);
    if (syms->name == NULL) {
        free(syms);
        return NULL;
    }

    syms->capacity = 0; 
    syms->entries = NULL;
    syms->nsymbols = 0;
    syms->refcnt = 1;
    return syms;
}


bool symbols_reserve(struct symbols *syms, size_t n)
{
    if (n <= syms->capacity) {
        return true;
    }

    size_t new_capacity = align_pow2(n);

    // Sanity check that we're not overflowing
    if ((new_capacity * sizeof(struct symbol*)) < (syms->capacity * sizeof(struct symbol*))) {
        return false;
    }

    log_ctx_push(LOG_CTX_NAME(syms->name));
    log_trace("Extending symbol table capacity to %zu", new_capacity);
    log_ctx_pop();

    struct symbol **entries = realloc(syms->entries, sizeof(struct symbol*) * new_capacity);
    if (entries == NULL) {
        return false;
    }

    // Zero out the new entries
    memset(&entries[syms->capacity], 0, (new_capacity - syms->capacity) * sizeof(struct symbol*));
    syms->entries = entries;
    syms->capacity = new_capacity;

    return true;
}


struct symbols * symbols_get(struct symbols *syms)
{
    assert(syms != NULL);
    assert(syms->refcnt > 0);
    ++(syms->refcnt);
    return syms;
}


void symbols_put(struct symbols *syms)
{
    assert(syms != NULL);
    assert(syms->refcnt > 0);

    if (--(syms->refcnt) == 0) {
        for (size_t i = 0; i < syms->capacity && syms->nsymbols > 0; ++i) {
            if (syms->entries[i] != NULL) {
                symbol_put(syms->entries[i]);
                syms->entries[i] = NULL;
                --(syms->nsymbols);
            }
        }
        free(syms->entries);
        free(syms->name);
        free(syms);
    }
}


int symbols_insert(struct symbols *syms, uint64_t idx, 
                   struct symbol *symbol, struct symbol **existing)
{
    struct symbol **pos = NULL;

    log_ctx_push(LOG_CTX_NAME(syms->name));

    if (idx >= syms->capacity) {
        if (!symbols_reserve(syms, idx + 1)) {
            log_error("Section index %llu is too large", idx);
            log_ctx_pop();
            return ENOMEM;
        }
    }

    pos = &(syms->entries[idx]);
    if (*pos != NULL) {
        log_error("Section table already contains a symbol with index %llu", idx);
        log_ctx_pop();
        if (existing != NULL) {
            *existing = *pos;
        }
        return EEXIST;
    }

    *pos = symbol_get(symbol);
    ++(syms->nsymbols);

    log_ctx_pop();
    return 0;
}
