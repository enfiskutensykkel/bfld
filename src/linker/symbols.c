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

    if (name != NULL) {
        syms->name = strdup(name);
    }
    syms->capacity = 0; 
    syms->symbols = NULL;
    syms->nsymbols = 0;
    syms->maxidx = 0;
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

    log_trace("Extending local symbol table capacity to %zu", new_capacity);

    struct symbol **symbols = realloc(syms->symbols, sizeof(struct symbol*) * new_capacity);
    if (symbols == NULL) {
        return false;
    }

    // Zero out the new entries
    memset(&symbols[syms->capacity], 0, (new_capacity - syms->capacity) * sizeof(struct symbol*));
    syms->symbols = symbols;
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


void symbols_clear(struct symbols *syms)
{
    for (size_t i = 0; i < syms->capacity && syms->nsymbols > 0; ++i) {
        if (syms->symbols[i] != NULL) {
            symbol_put(syms->symbols[i]);
            syms->symbols[i] = NULL;
            syms->nsymbols--;
        }
    }
    syms->maxidx = 0;
}


void symbols_put(struct symbols *syms)
{
    assert(syms != NULL);
    assert(syms->refcnt > 0);

    if (--(syms->refcnt) == 0) {
        symbols_clear(syms);
        free(syms->symbols);
        if (syms->name != NULL) {
            free(syms->name);
        }
        free(syms);
    }
}


uint64_t symbols_push(struct symbols *syms, struct symbol *sym)
{
    uint64_t idx;
    uint64_t maxidx = syms->maxidx;

    do {
        idx = ++maxidx;

        if (idx >= syms->capacity) {
            if (!symbols_reserve(syms, idx + 1)) {
                return 0;
            }
        }

    } while (syms->symbols[idx] != NULL);

    syms->symbols[idx] = symbol_get(sym);
    syms->nsymbols++;
    syms->maxidx = maxidx;
    return idx;
}


int symbols_insert(struct symbols *syms, uint64_t idx, 
                   struct symbol *symbol, struct symbol **existing)
{
    struct symbol **pos = NULL;

    if (idx >= syms->capacity) {
        if (!symbols_reserve(syms, idx + 1)) {
            log_error("Symbol index %llu is too large", idx);
            return ENOMEM;
        }
    }

    pos = &(syms->symbols[idx]);
    if (*pos != NULL) {
        log_error("Local symbol table already contains a symbol with index %llu", idx);
        if (existing != NULL) {
            *existing = *pos;
        }
        return EEXIST;
    }

    *pos = symbol_get(symbol);
    syms->nsymbols++;

    if (idx > syms->maxidx) {
        syms->maxidx = idx;
    }

    return 0;
}


bool symbols_remove(struct symbols *syms, uint64_t idx)
{
    if (idx >= syms->capacity) {
        return false;
    }

    struct symbol **sym = &(syms->symbols[idx]);
    if (*sym == NULL) {
        return false;
    }

    symbol_put(*sym);
    *sym = NULL;
    syms->nsymbols--;

    if (idx == syms->maxidx) {
        while (syms->maxidx > 0 && syms->symbols[syms->maxidx] == NULL) {
            syms->maxidx--;
        }
    }

    return true;
}


struct symbol * symbols_pop(struct symbols *syms)
{
    if (syms->maxidx > 0) {
        assert(syms->maxidx < syms->capacity);
        assert(syms->symbols[syms->maxidx] != NULL);

        struct symbol *sym = symbol_get(syms->symbols[syms->maxidx]);
        symbols_remove(syms, syms->maxidx);
        return sym;
    }
    return NULL;
}
