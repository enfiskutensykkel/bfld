#include "sections.h"
#include "utils/deque.h"
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>


struct sections * sections_alloc(uint64_t n)
{
    struct sections *s = malloc(sizeof(struct sections));
    if (s == NULL) {
        return NULL;
    }

    s->refcnt = 0;
    s->nsections = 0;
    s->q = DEQUE_INIT;
    sections_reserve(s, n);
    return s;
}


struct sections * sections_get(struct sections *sq)
{
    assert(sq != NULL);
    assert(sq->refcnt > 0);
    sq->refcnt++;
    return sq;
}


void sections_put(struct sections *sq)
{
    assert(sq != NULL);
    assert(sq->refcnt > 0);

    if (--(sq->refcnt) == 0) {
        sections_clear(sq);
        free(sq);
    }
 }
