#ifndef BFLD_UTILS_DEQUE_H
#define BFLD_UTILS_DEQUE_H
#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdatomic.h>
#include "spinlock.h"
#include "cdefs.h"


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
    size_t head;
    _Atomic size_t size;
    size_t capacity;
    struct spinlock lock;
};


/*
 * Initialize an empty deque.
 */
#define DEQUE_INIT (struct deque) {NULL, 0, 0, 0, SPINLOCK_INIT}


/*
 * Initialize an empty deque.
 */
static inline
void deque_init(struct deque *d)
{
    d->q = NULL;
    d->head = 0;
    atomic_store_explicit(&d->size, 0, memory_order_relaxed);
    d->capacity = 0;
    spinlock_init(&d->lock);
}


/*
 * Clear the deque and free the buffer.
 */
void deque_clear(struct deque *d);


/*
 * Helper function to reserve space in the internal deque buffer.
 */
bool deque_reserve_unlocked(struct deque *d, size_t capacity);


/*
 * Get the number of entries in the deque.
 */
static inline
size_t deque_size(const struct deque *d)
{
    return atomic_load_explicit(&d->size, memory_order_relaxed);
}


/*
 * Is the deque empty?
 */
static inline
bool deque_empty(const struct deque *d)
{
    return deque_size(d) == 0;
}


/*
 * Reserve space in the internal deque buffer.
 * Returns true if the deque is able to hold at least
 * capacity entries, and false otherwise.
 */
static inline
bool deque_reserve(struct deque *d, size_t capacity)
{
    bool status;
    spinlock_lock(&d->lock);
    status = deque_reserve_unlocked(d, capacity);
    spinlock_unlock(&d->lock);
    return status;
}


/*
 * Helper function to push an entry to the back of the deque (tail insert).
 * Returns true if the entry was inserted, and false otherwise.
 */
static inline
bool deque_push_back_unlocked(struct deque *d, void *entry)
{
    if (unlikely(deque_size(d) == d->capacity)) {
        if (!deque_reserve_unlocked(d, d->capacity != 0 ? d->capacity * 2 : 8)) {
            return false;
        }
    }

    size_t size = atomic_load_explicit(&d->size, memory_order_relaxed);
    size_t tail = (d->head + size) & (d->capacity - 1);
    d->q[tail] = entry;
    atomic_store_explicit(&d->size, size + 1, memory_order_relaxed);
    return true;
}


/*
 * Push an entry to the back of the deque (tail insert).
 * Returns true if the entry was inserted, and false otherwise.
 */
static inline
bool deque_push_back(struct deque *d, void *entry)
{
    spinlock_lock(&d->lock);
    bool status = deque_push_back_unlocked(d, entry);
    spinlock_unlock(&d->lock);
    return status;
}



/*
 * Helper function to push an entry to the back of the deque (head insert).
 * Returns true if the entry was inserted, and false otherwise.
 */
static inline
bool deque_push_front_unlocked(struct deque *d, void *entry)
{
    if (unlikely(deque_size(d) == d->capacity)) {
        if (!deque_reserve_unlocked(d, d->capacity != 0 ? d->capacity * 2 : 8)) {
            return false;
        }
    }

    d->head--;
    d->q[d->head & (d->capacity - 1)] = entry;
    size_t size = atomic_load_explicit(&d->size, memory_order_relaxed);
    atomic_store_explicit(&d->size, size + 1, memory_order_relaxed);
    return true;
}


/*
 * Push an entry to the back of the deque (head insert).
 * Returns true if the entry was inserted, and false otherwise.
 */
static inline
bool deque_push_front(struct deque *d, void *entry)
{
    spinlock_lock(&d->lock);
    bool status = deque_push_front(d, entry);
    spinlock_unlock(&d->lock);
    return status;
}


/*
 * Helper function to pop off an entry from the front of the deque (head remove).
 * Returns the entry or NULL if size is 0.
 */
static inline
void * deque_pop_front_unlocked(struct deque *d)
{
    void *entry = NULL;

    spinlock_lock(&d->lock);
    size_t size = deque_size(d);
    if (likely(size != 0)) {
        entry = d->q[d->head & (d->capacity - 1)];
        d->head++;
        atomic_store_explicit(&d->size, size - 1, memory_order_relaxed);
    }
    spinlock_unlock(&d->lock);

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

    spinlock_lock(&d->lock);
    size_t size = deque_size(d);
    if (likely(size != 0)) {
        size_t tail = d->head + size - 1;
        entry = d->q[tail & (d->capacity - 1)];
        atomic_store_explicit(&d->size, size - 1, memory_order_relaxed);
    }
    spinlock_unlock(&d->lock);

    return entry;
}


/*
 * Peek at the first entry in the deque.
 */
static inline
void * deque_front(struct deque *d)
{
    void *entry = NULL;

    spinlock_lock(&d->lock);
    if (likely(deque_size(d) > 0)) {
        entry = d->q[d->head & (d->capacity - 1)];
    }
    spinlock_unlock(&d->lock);

    return entry;
}


/*
 * Peek at the first entry in the deque.
 */
static inline
void * deque_head(struct deque *d)
{
    return deque_front(d);
}


/*
 * Peek at the last entry in the deque.
 */
static inline
void * deque_back(struct deque *d)
{
    void *entry = NULL;

    spinlock_lock(&d->lock);
    size_t size = deque_size(d);
    if (likely(size > 0)) {
        size_t tail = d->head + size - 1;
        entry = d->q[tail & (d->capacity - 1)];
    }
    spinlock_unlock(&d->lock);

    return entry;
}


/*
 * Peek at the last entry in the deque.
 */
static inline
void * deque_tail(struct deque *d)
{
    return deque_back(d);
}


/*
 * Peek at the entry at the given position relative to the head/first entry.
 */
static inline
void * deque_peek(struct deque *d, size_t position)
{
    void *entry = NULL;

    spinlock_lock(&d->lock);
    if (likely(position < deque_size(d))) {
        entry = d->q[(d->head + position) & (d->capacity - 1)];
    }
    spinlock_unlock(&d->lock);

    return entry;
}


#ifdef __cplusplus
}
#endif
#endif
