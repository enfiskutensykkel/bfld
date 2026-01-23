#include "deque.h"
#include "align.h"
#include <stdlib.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>


bool deque_reserve(struct deque *d, uint64_t capacity)
{
    if (capacity <= d->capacity) {
        return true;
    }

    if (capacity < 8) {
        capacity = 8;
    }

    // Make sure capacity is aligned to a power of two
    capacity = align_pow2(capacity);

    // Naive check for overflow
    if (capacity * sizeof(void*) < d->capacity * sizeof(void*)) {
        return false;
    }

    void **q = realloc(d->q, sizeof(void*) * capacity);
    if (q == NULL) {
        return false;
    }

    if (d->head > 0) {
        uint64_t n = d->capacity - d->head;
        uint64_t head = capacity - n;
        memmove(&q[head], &q[d->head], n * sizeof(void*));
        d->head = head;
    }

    d->q = q;
    d->capacity = capacity;
    return true;
}


void deque_clear(struct deque *d)
{
    if (d->q != NULL) {
        free(d->q);
        d->q = NULL;
    }
    deque_init(d);
}
