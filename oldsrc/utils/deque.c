#include "spinlock.h"
#include "deque.h"
#include "align.h"
#include "cdefs.h"
#include <stdlib.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>


bool deque_reserve_unlocked(struct deque *d, size_t capacity)
{
    if (unlikely(capacity <= d->capacity)) {
        return true;
    }

    if (unlikely(capacity < 8)) {
        capacity = 8;
    }

    // Make sure capacity is aligned to a power of two
    capacity = align_roundup(capacity);
    if (capacity == SIZE_MAX) {
        return false;
    }

    // Naive check for overflow
    if (capacity * sizeof(void*) < d->capacity * sizeof(void*)) {
        return false;
    }

    void **q = realloc(d->q, sizeof(void*) * capacity);
    if (q == NULL) {
        return false;
    }

    // If the deque has wrapped, we need to move some entries to the new gap
    size_t head = d->head & (d->capacity - 1);
    size_t size = atomic_load_explicit(&d->size, memory_order_relaxed);
    if ((head + size) > d->capacity) {
        size_t n = d->capacity - head;
        size_t new_head = capacity - n;
        memmove(&q[new_head], &q[head], n * sizeof(void*));
        d->head = new_head;
    } else {
        // normalize head to new capacity
        d->head = head;
    }

    d->q = q;
    d->capacity = capacity;
    return true;
}


void deque_clear(struct deque *d)
{
    spinlock_lock(&d->lock);
    if (d->q != NULL) {
        free(d->q);
        d->q = NULL;
    }
    d->head = 0;
    atomic_store_explicit(&d->size, 0, memory_order_relaxed);
    d->capacity = 0;
    spinlock_unlock(&d->lock);
}
