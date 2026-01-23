#include "deque.h"
#include "align.h"
#include <stdlib.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>


void deque_clear(struct deque *d)
{
    if (d->q != NULL) {
        free(d->q);
        d->q = NULL;
    }
    deque_init(d);
}


bool deque_reserve(struct deque *d, size_t capacity)
{
    if (capacity <= d->capacity) {
        return true;
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
        size_t n = d->capacity - d->head;
        size_t head = capacity - n;
        memmove(&q[head], &q[d->head], n * sizeof(void*));
        d->head = head;
    }

    d->q = q;
    d->capacity = capacity;
    return true;
}


bool deque_push_back(struct deque *d, void *entry)
{
    if (d->size == d->capacity) {
        if (!deque_reserve(d, d->capacity != 0 ? d->capacity * 2 : 8)) {
            return false;
        }
    }

    size_t tail = (d->head + d->size) & (d->capacity - 1);
    d->q[tail] = entry;
    d->size++;
    return true;
}


bool deque_push_front(struct deque *d, void *entry)
{
    if (d->size == d->capacity) {
        if (!deque_reserve(d, d->capacity != 0 ? d->capacity * 2 : 8)) {
            return false;
        }
    }

    d->head--;
    d->q[d->head & (d->capacity - 1)] = entry;
    d->size++;
    return true;
}


void * deque_pop_front(struct deque *d)
{
    void *entry = NULL;

    if (d->size != 0) {
        entry = d->q[d->head & (d->capacity - 1)];
        d->head++;
        d->size--;
    }

    return entry;
}


void * deque_pop_back(struct deque *d)
{
    void *entry = NULL;

    if (d->size != 0) {
        size_t tail = d->head + d->size - 1;
        entry = d->q[tail & (d->capacity - 1)];
        d->size--;
    }

    return entry;
}
