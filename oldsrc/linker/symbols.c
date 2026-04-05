#include "symbols.h"
#include "utils/deque.h"
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>


struct symbols * symbols_alloc(uint64_t n)
{
    struct symbols *s = malloc(sizeof(struct symbols));
    if (s == NULL) {
        return NULL;
    }

    s->refcnt = 0;
    s->nsymbols = 0;
    s->q = DEQUE_INIT;
    symbols_reserve(s, n);
    return s;
}


struct symbols * symbols_get(struct symbols *sq)
{
    assert(sq != NULL);
    assert(sq->refcnt > 0);
    sq->refcnt++;
    return sq;
}


void symbols_put(struct symbols *sq)
{
    assert(sq != NULL);
    assert(sq->refcnt > 0);

    if (--(sq->refcnt) == 0) {
        symbols_clear(sq);
        free(sq);
    }
 }
