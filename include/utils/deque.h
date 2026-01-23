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
 * This implementation uses a dynamic array with potential O(n)
 * grow complexity. The user should preferably reserve the maximum
 * amount of entries if this is known in advance to avoid costly
 * grows.
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
 * Declare a type-safe deque with a given name.
 */
#define DEQUE_DECLARE(type, name) \
    struct name { \
        type **q; \
        uint64_t head; \
        uint64_t size; \
        uint64_t capacity; \
    }; \
    \
    static inline \
    bool name##_reserve(struct name *d, uint64_t capacity) \
    { \
        return deque_reserve((struct deque*) d, capacity); \
    } \
    static inline \
    void name##_init(struct name *d) \
    { \
        deque_init((struct deque*) d); \
    } \
    static inline \
    void name##_clear(struct name *d) \
    { \
        deque_clear((struct deque*) d); \
    } \
    static inline \
    bool name##_push_back(struct name *d, type *entry) \
    { \
        deque_push_back((struct deque*) d, (void*) entry); \
    } \
    static inline \
    bool name##_push_front(struct name *d, type *entry) \
    { \
        deque_push_front((struct deque*) d, (void*) entry); \
    } \
    static inline \
    type * name##_pop_front(struct name *d) \
    { \
        return (type*) deque_pop_front((struct deque*) d); \
    } \
    static inline \
    type * name##_pop_back(struct name *d) \
    { \
        return (type*) deque_pop_back((struct deque*) d); \
    } \
    static inline \
    bool name##_empty(const struct name *d) \
    { \
        return d->size == 0; \
    } \
    static inline \
    uint64_t name##_size(const struct name *d) \
    { \
        return d->size; \
    }


#ifdef __cplusplus
}
#endif
#endif
