#ifndef BFLD_ARENA_ALLOCATOR_H
#define BFLD_ARENA_ALLOCATOR_H
#ifdef __cplusplus
extern "C" {
#endif

#include "cdefs.h"
#include "align.h"
#include "valgrind.h"
#include <stddef.h>
#include <stdbool.h>
#include <string.h>


#define ARENA_DEFAULT_CAPACITY (2ULL << 20)


/*
 * Concurrent arena allocator - a lock-free bump allocator.
 *
 * Allocates a memory region in advance using mmap() with MAP_NORESERVE,
 * and provides a mechanism for lock free bump allocation within that region.
 * This allows very fast allocations for objects that have similar lifetimes, 
 * rather than tracking them manually with malloc/free.
 *
 * Note that even though this arena allocator supports atomic bump allocation,
 * it is still suboptimal to have contention for a single, shared arena.
 * Ideally use a thread-local arena, where each thread manages its own arena
 * allocator, in order to avoid contention.
 */
struct arena
{
    alignas(64) _Atomic(size_t) used;           // number of bytes currently used in the arena
    size_t capacity;                            // total capacity of the memory region
    uint8_t *base;                              // pointer to the start of the memory region
    char pad[64 - sizeof(_Atomic(size_t)) - sizeof(size_t) - sizeof(uint8_t*)];
    alignas(64) _Atomic(struct arena*) next;    // next arena pointer
    _Atomic(uint32_t) refcnt;                   // reference counter
};


/*
 * Linked-list of arenas, in order to track multiple arenas.
 */
struct arena_list
{
    alignas(64) _Atomic(struct arena*) head;
};


/*
 * Initialize the arena list.
 */
#define ARENA_LIST_INIT (struct arena_list) {NULL}


/*
 * Initialize the arena list.
 */
static inline
void arena_list_init(struct arena_list *list)
{
    atomic_store_explicit(&list->head, NULL, memory_order_relaxed);
}


/*
 * Helper function to get the capacity (number of bytes) of an arena.
 */
static inline
size_t arena_capacity(const struct arena *arena)
{
    if (likely(arena != NULL)) {
        return arena->capacity;
    }
    return 0;
}


/*
 * Helper function to get the number of used bytes in an arena.
 */
static inline
size_t arena_used(const struct arena *arena)
{
    if (likely(arena != NULL)) {
        return atomic_load_explicit(&arena->used, memory_order_relaxed);
    }
    return 0;
}


/*
 * Helper function to get the number of bytes remaining in an arena.
 */
static inline
size_t arena_remaining(const struct arena *arena)
{
    if (likely(arena != NULL)) {
        size_t used = atomic_load_explicit(&arena->used, memory_order_relaxed);
        return arena->capacity - used;
    }
    return 0;
}


/*
 * Create an arena with (at least) the specified capacity.
 * This maps an underlying memory region with mmap() and MAP_NORESERVE,
 * allowing the memory to be lazily allocated on demand.
 */
struct arena * arena_create(size_t capacity);


/*
 * Destroy the arena.
 * Releases the underlying memory region and free the arena pointer.
 */
void arena_destroy(struct arena *arena);


/*
 * Take an arena reference.
 *
 * Increases the arena's reference count and ensures that the underlying
 * memory region is not released while the reference is held.
 */
static inline
struct arena * arena_get(struct arena *arena)
{
    if (likely(arena != NULL)) {
        atomic_fetch_add_explicit(&arena->refcnt, 1, memory_order_relaxed);
    }
    return arena;
}


/*
 * Release the reference to an arena.
 * Decrement the arena's reference count. If the reference count 
 * becomes zero, the arena is destroyed.
 *
 * After calling this, you must not use the arena as it may have been destroyed.
 */
void arena_put(struct arena *arena)
{
    if (atomic_fetch_sub_explicit(&arena->refcnt, 1, memory_order_acq_rel) == 1) {
        arena_destroy(arena);
    }
}


/*
 * Clear an arena list.
 *
 * Empties the list and releases all arenas in it, by calling
 * arena_put(). This can be used to implement deferred free/retire
 * for arenas.
 */
static inline
void arena_list_clear(struct arena_list *list)
{
    struct arena *head = atomic_exchange_explicit(&list->head, NULL, memory_order_acq_rel);
    
    while (head != NULL) {
        struct arena *next = atomic_load_explicit(&head->next, memory_order_relaxed);
        atomic_store_explicit(&head->next, NULL, memory_order_relaxed);
        arena_put(head);
        head = next;
    }
}


/*
 * Move all arenas in an arena list to a different list.
 */
static inline
void arena_list_move(struct arena_list *new_list, struct arena_list *old_list)
{
    struct arena *head, *tail, *next;

    head = atomic_exchange_explicit(&list->head, NULL, memory_order_acq_rel);
    
    if (head == NULL) {
        return;
    }

    // Find the list tail
    tail = head, next = atomic_load_explicit(&head->next, memory_order_relaxed);
    while (next != NULL) {
        tail = next;
        next = atomic_load_explicit(&tail->next, memory_order_relaxed);
    }

    // Try to replace the head in the new list
    next = atomic_load_explicit(&new_list->head, memory_order_acquire);
    do {
        atomic_store_explicit(&tail->next, next, memory_order_relaxed);
    } while (!atomic_compare_exchange_weak_explicit(&new_list->head, &next, head,
                                                    memory_order_release,
                                                    memory_order_acquire));
}


/*
 * Insert an arena into an arena list.
 *
 * This can be used to "retire" arenas, inserting them into a limbo list
 * for deferred destruction.
 *
 * This is essentially the same as releasing ownership of the arena,
 * and should be treated the same way as arena_put(); the arena 
 * must not be used after calling this function as it may be destroyed
 * at any point by another thread calling arena_list_clear().
 */
static inline
void arena_list_add(struct arena_list *list, struct arena *arena)
{
    struct arena *head = atomic_load_explicit(&list->head, memory_order_acquire);

    do {
        atomic_store_explicit(&arena->next, head, memory_order_relaxed);
    } while (!atomic_compare_exchange_weak_explicit(&list->head, &head, arena,
                                                    memory_order_release,
                                                    memory_order_acquire));
}


/*
 * Retire the arena by adding it to a limbo list.
 */
static inline
void arena_put_deferred(struct arena *arena, struct arena_list *limbo)
{
    arena_list_add(limbo, arena);
}


/*
 * Alloate a memory block of (at least) the specified size
 * in the arena, starting at the specified alignment.
 *
 * Tries to reserve a block within the arena and returns a non-NULL
 * pointer to the reserved memory block. The pointer is aligned to
 * the specified alignment (which must be a power of two).
 *
 * If the specified size and alignment exceeds the remaining number 
 * of free bytes, this function returns NULL.
 *
 * On success, this function returns a non-NULL pointer to the
 * reserved memory block. On failure, this function returns NULL.
 *
 * The reserved memory is initialized to zero.
 *
 * This function is thread-safe, allowing two or more threads/tasks
 * to use the same arena under contention. However, the user should
 * be aware of contention-related performance implications, such as
 * cache line contention.
 */
static inline
void * arena_alloc(struct arena *arena, size_t size, size_t alignment)
{
    size_t used, offset;
    uintptr_t base, addr;

    if (unlikely(arena == NULL)) {
        return NULL;
    }

    // Even though it would be technically more correct to
    // check for overflow using the calculated start offset,
    // we want to avoid doing that in the loop
    if (unlikely(size > arena->capacity) || unlikely(arena->capacity > SIZE_MAX - size)) {
        return NULL;
    }

    base = (uintptr_t) arena->base;
    used = atomic_load_explicit(&arena->used, memory_order_acquire);

    for (;;) {
        addr = align_to(base + used, alignment);
        offset = (size_t) (addr - base);

        if (unlikely(offset + size > arena->capacity)) {
            // There is not enough space left in the arena for the allocation
            return NULL;
        }

        if (atomic_compare_exchange_weak_explicit(&arena->used, &used, offset + size,
                                                  memory_order_release,
                                                  memory_order_acquire)) {
            VALGRIND_MAKE_MEM_DEFINED((void*) (arena->base + offset), size);
            return (void*) (arena->base + offset);
        }
    }
}


/*
 * Alloate a memory block of (at least) the specified size
 * in the arena, starting at the specified alignment.
 *
 * Tries to reserve a block within the arena and returns a non-NULL
 * pointer to the reserved memory block. The pointer is aligned to
 * the specified alignment (which must be a power of two).
 *
 * If the specified size and alignment exceeds the remaining number 
 * of free bytes, this function returns NULL.
 *
 * On success, this function returns a non-NULL pointer to the
 * reserved memory block. On failure, this function returns NULL.
 *
 * The reserved memory is initialized to zero.
 *
 * Note that this function is NOT thread-safe, as it assumes exclusive access
 * by a single thread. If the arena is shared, use arena_alloc() instead.
 */
static inline
void * arena_alloc_local(struct arena *arena, size_t size, size_t alignment)
{
    if (unlikely(arena == NULL)) {
        return NULL;
    }

    uintptr_t base = (uintptr_t) arena->data;
    size_t used = atomic_load_explicit(&arena->used, memory_order_relaxed);

    uintptr_t addr = align_to(base + used, align);
    size_t offset = (size_t) (addr - base);

    if (unlikely(size > SIZE_MAX - offset) || unlikely(offset + size > arena->size)) {
        return NULL;
    }

    atomic_store_explicit(&arena->used, offset + size, memory_order_relaxed);
    VALGRIND_MAKE_MEM_DEFINED((void*) (arena->base + offset), size);
    return (void*) (arena->base + offset);
}


/*
 * 
 */
static inline
void * arena_alloc_local_growable(struct arena_list *list,
                                  size_t size, 
                                  size_t alignment)
{
    void *ptr;
    struct arena *head = atomic_load_explicit(&pool->current, memory_order_acquire);
    size_t capacity;
    
    if (likely(head != NULL)) {
        capacity = head->capacity;
    } else {
        capacity = ARENA_DEFAULT_CAPACITY;
    }

    if (unlikely(align_to(size, alignment) > capacity)) {
        // The specified size is larger than the default arena size
        capacity = size;

    } else {
        // Try to reserve block in the current arena
        ptr = arena_alloc(head, size, alignment);
        if (likely(ptr != NULL)) {
            return ptr;
        }

        arean_list_add(&pool->list, head);
    }

    // We could not fit the block into the current arena (or current arena is NULL)
    // Allocate a new arena with the same size as the previous arena
    head = arena_create(capacity);
    if (unlikely(head == NULL)) {
        *current = NULL;
        return NULL;
    }

    // We can use thread-local variant here as the arena is not added to the list yet
    ptr = arena_alloc_local(head, size, alignment);
    *current = head;
    return ptr;
}


#ifdef __cplusplus
}
#endif
#endif
