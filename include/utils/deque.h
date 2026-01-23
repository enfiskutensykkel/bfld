#ifndef BFLD_UTILS_DEQUE_H
#define BFLD_UTILS_DEQUE_H
#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>


struct deque
{
    void **q;
    size_t head;
    size_t size;
    size_t capacity;
};


#define DEQUE_INIT (struct deque) {NULL, 0, 0, 0}


static inline
void deque_init(struct deque *d)
{
    d->q = NULL;
    d->head = 0;
    d->size = 0;
    d->capacity = 0;
}


void deque_clear(struct deque *d);


bool deque_reserve(struct deque *d, size_t capacity);


bool deque_push_back(struct deque *d, void *entry);


bool deque_push_front(struct deque *d, void *entry);


void * deque_pop_front(struct deque *d);


void * deque_pop_back(struct deque *d);


static inline
size_t deque_size(const struct deque *d)
{
    return d->size;
}


#define named_deque(type, name) \
    struct name { \
        type **q; \
        size_t head; \
        size_t size; \
        size_t capacity; \
    }; \
    \
    static inline \
    bool name##_reserve(struct name *d, size_t capacity) \
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
    size_t name##_size(const struct name *d) \
    { \
        return d->size; \
    }


#ifdef __cplusplus
}
#endif
#endif
