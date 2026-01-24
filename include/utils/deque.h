#ifndef BFLD_UTILS_DEQUE_H
#define BFLD_UTILS_DEQUE_H
#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


/*
 * Double-ended queue (deque).
 *
 * Double-ended queues are useful for O(1) tail and head insertions,
 * and can be used to implement FIFO and LIFO queues.
 *
 * This implementation uses a dynamic array with potential O(n) worst
 * case grow complexity. The user should preferably reserve the maximum
 * amount of entries if this is known in advance to avoid costly
 * grows.
 *
 * This deque implementation may wrap around, meaning iterating it is 
 * not cache-friendly. Ues this structure primarily if O(1) insertions
 * and deletions are the primary use-case.
 */
struct deque
{
    void **q;
    uint64_t head;
    uint64_t size;
    uint64_t capacity;
};


/*
 * Initialize an empty deque.
 */
#define DEQUE_INIT (struct deque) {NULL, 0, 0, 0}


/*
 * Initialize an empty deque.
 */
static inline
void deque_init(struct deque *d)
{
    d->q = NULL;
    d->head = 0;
    d->size = 0;
    d->capacity = 0;
}


/*
 * Clear the deque and free the buffer.
 */
void deque_clear(struct deque *d);


/*
 * Reserve space in the internal deque buffer.
 * Returns true if the deque is able to hold at least
 * capacity entries, and false otherwise.
 */
bool deque_reserve(struct deque *d, uint64_t capacity);


/*
 * Push an entry to the back of the deque (tail insert).
 * Returns true if the entry was inserted, and false otherwise.
 */
static inline
bool deque_push_back(struct deque *d, void *entry)
{
    if (d->size == d->capacity) {
        if (!deque_reserve(d, d->capacity != 0 ? d->capacity * 2 : 8)) {
            return false;
        }
    }

    uint64_t tail = (d->head + d->size) & (d->capacity - 1);
    d->q[tail] = entry;
    d->size++;
    return true;
}


/*
 * Push an entry to the back of the deque (head insert).
 * Returns true if the entry was inserted, and false otherwise.
 */
static inline
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


/*
 * Pop off an entry from the front of the deque (head remove).
 * Returns the entry or NULL if size is 0.
 */
static inline
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


/*
 * Pop off an entry from the back of the deque (tail remove).
 * Returns the entry or NULL if size is 0.
 */
static inline
void * deque_pop_back(struct deque *d)
{
    void *entry = NULL;

    if (d->size != 0) {
        uint64_t tail = d->head + d->size - 1;
        entry = d->q[tail & (d->capacity - 1)];
        d->size--;
    }

    return entry;
}


/*
 * Get the number of entries in the deque.
 */
static inline
uint64_t deque_size(const struct deque *d)
{
    return d->size;
}


/*
 * Is the deque empty?
 */
static inline
bool deque_empty(const struct deque *d)
{
    return d->size == 0;
}


/*
 * Peek at the first entry in the deque.
 */
static inline
void * deque_front(const struct deque *d)
{
    void *entry = NULL;

    if (d->size > 0) {
        entry = d->q[d->head & (d->capacity - 1)];
    }

    return entry;
}


/*
 * Peek at the first entry in the deque.
 */
static inline
void * deque_head(const struct deque *d)
{
    return deque_front(d);
}


/*
 * Peek at the last entry in the deque.
 */
static inline
void * deque_back(const struct deque *d)
{
    void *entry = NULL;

    if (d->size > 0) {
        uint64_t tail = d->head + d->size - 1;
        entry = d->q[tail & (d->capacity - 1)];
    }

    return entry;
}


/*
 * Peek at the last entry in the deque.
 */
static inline
void * deque_tail(const struct deque *d)
{
    return deque_back(d);
}


/*
 * Peek at the entry at the given position relative to the head/first entry.
 */
static inline
void * deque_peek(const struct deque *d, uint64_t position)
{
    void *entry = NULL;

    if (position < d->size) {
        entry = d->q[(d->head + position) & (d->capacity - 1)];
    }

    return entry;
}


#ifdef __cplusplus
}
#endif
#endif
