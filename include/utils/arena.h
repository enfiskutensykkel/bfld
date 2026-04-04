#ifndef BFLD_UTILS_ARENA_ALLOCATOR_H
#define BFLD_UTILS_ARENA_ALLOCATOR_H
#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdalign.h>
#include <stdatomic.h>
#include <string.h>
#include "align.h"
#include "cdefs.h"

#if defined(HAS_VALGRIND) && !defined(NDEBUG)
#include <valgrind/memcheck.h>
#else
#define VALGRIND_MAKE_MEM_UNDEFINED(addr, len) (void) 0
#endif


/*
 * Default arena size
 */
#define ARENA_DEFAULT_SIZE  (2ULL << 20)


/*
 * Arena allocator - a lock free, region-based memory allocator.
 *
 * Allocates a memory region in advance and provides a mechanism for 
 * lock free bump allocation within that region. This allows very 
 * fast allocation for objects that have similar lifetimes, rather 
 * than tracking memory manually with malloc/free.
 *
 * Note that even though this arena supports atomic bump allocation,
 * it is suboptimal to have contention for a single, shared arena.
 * Ideally threads should work on their own, thread-local arenas.
 */
struct arena
{
    size_t _Atomic used;            // number of bytes currently used
    size_t size;                    // total size of the memory region
    uint8_t *data;                  // pointer to the actual memory
    struct arena * _Atomic next;    // pointer to the next memory region
};


/*
 * Get the total memory size of the arena (number of bytes allocated).
 */
static inline
size_t arena_size(const struct arena *arena)
{
    if (likely(arena != NULL)) {
        return arena->size;
    }

    return 0;
}


/*
 * Get the number of bytes currently used in the arena.
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
 * Get the number of bytes remaining in an arena.
 */
static inline
size_t arena_unused(const struct arena *arena)
{
    if (likely(arena != NULL)) {
        size_t used = atomic_load_explicit(&arena->used, memory_order_relaxed);
        return arena->size - used;
    }

    return 0;
}


/*
 * Track all arenas in a list, allowing arenas to be destroyed and
 * memory to be freed once at the end of the program.
 */
struct arena_list
{
    struct arena * _Atomic head;
};


/*
 * Initialize the arena list with a default arena size.
 */
static inline
void arena_list_init(struct arena_list *list)
{
    atomic_init(&list->head, NULL);
}


#define ARENA_LIST_INIT (struct arena_list) {NULL}


/*
 * Allocate a memory block of (at least) the specified size within an arena,
 * using the specified alignment.
 *
 * Tries to reserve a block within the specified arena. On success,
 * this function returns a non-NULL pointer to the reserved memory block.
 * The pointer is aligned to the specified alignment (which must be 
 * a power of two). On failure, this function returns NULL.
 *
 * This function is thread-safe, allowing two or more threads/tasks
 * to use the same arena under contention. However, the user should
 * beware of problems related to contention, particularly cache 
 * contention.
 */
static inline
void * arena_alloc_threadsafe(struct arena *arena, size_t size, size_t align)
{
    size_t used, offset;
    uintptr_t base, addr;

    if (unlikely(arena == NULL)) {
        return NULL;
    }

    // Even though the correct check for overflow is
    // using offset, we don't want to do that in the loop
    if (unlikely(size > arena->size) || unlikely(arena->size > SIZE_MAX - size)) {
        return NULL;
    }

    base = (uintptr_t) arena->data;
    used = atomic_load_explicit(&arena->used, memory_order_acquire);

    for (;;) {
        addr = align_to(base + used, align);
        offset = (size_t) (addr - base);

        if (unlikely(offset + size > arena->size)) {
            return NULL;
        }

        if (atomic_compare_exchange_weak_explicit(&arena->used, &used, offset + size,
                                                  memory_order_release,
                                                  memory_order_acquire)) {
            VALGRIND_MAKE_MEM_UNDEFINED(&arena->data[offset], size);
            return &arena->data[offset];
        }
    } 

    return NULL;
}


/*
 * Identical to arena_alloc_threadsafe except that the 
 * allocated memory is guaranteed to be initialized to zero.
 */
static inline
void * arena_alloc_threadsafe_zeroed(struct arena *arena, size_t size, size_t align)
{
    void *ptr = arena_alloc_threadsafe(arena, size, align);
    memset(ptr, 0, size);
    return ptr;
}


/*
 * Allocate a memory block of (at least) the specified size within an arena,
 * using the specified alignment.
 *
 * Tries to reserve a block within the specified arena. On success,
 * this function returns a non-NULL pointer to the reserved memory block.
 * The pointer is aligned to the specified alignment (which must be 
 * a power of two). On failure, this function returns NULL.
 *
 * Unlike arena_alloc_threadsafe, this function should NOT be
 * used when the arena is shared between multiple threads.
 */
static inline
void * arena_alloc(struct arena *arena, size_t size, size_t align)
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
    VALGRIND_MAKE_MEM_UNDEFINED(&arena->data[offset], size);
    return &arena->data[offset];
}


/*
 * Reset the arena's used counter.
 */
static inline
void arena_reset(struct arena *arena)
{
    if (likely(arena != NULL)) {
        atomic_store_explicit(&arena->used, 0, memory_order_release);
    }
}


/*
 * Identical to arena_alloc except that the allocated
 * memory is guaranteed to be initialized to zero.
 */
static inline
void * arena_alloc_zeroed(struct arena *arena, size_t size, size_t align)
{
    void *ptr = arena_alloc(arena, size, align);
    memset(ptr, 0, size);
    return ptr;
}


/*
 * Create an arena of the specified size and add it to the arena list
 *
 * This function allocates the underlying memory region used by the arena
 * using mmap() with the MAP_ANONYMOUS and MAP_PRIVATE flags. This allows 
 * memory to be lazily paged in only when it is actually used. 
 *
 * Returns the newly created arena, or NULL if allocation failed.
 */
struct arena * arena_list_add(struct arena_list *list, size_t size, size_t align);


/*
 * Destroy all arenas tracked in the specified list,
 * and free their underlying memory regions.
 *
 * Note that arenas should only be destroyed once they
 * are guaranteed to no longer be in use.
 */
void arena_list_free(struct arena_list *list);


/*
 * Allocate a memory block of (at least) the specified size,
 * using the specified alignment.
 *
 * Tries to reserve a block of the specified size. On success, this function 
 * returns a non-NULL pointer to the reserved memory block. The pointer is 
 * aligned to the specified alignment (which must be  a power of two). 
 * On failure, this function returns NULL.
 *
 * This function will attempt to reserve the block within the arena
 * specified by the current pointer. If there is not enough space,
 * this function will create a new arena and allocate from this,
 * and update the current pointer. Each thread/task should use
 * their own current arena pointer.
 */
static inline
void * arena_alloc_dynamic(struct arena_list *list, struct arena **current, size_t size, size_t align)
{
    void *ptr = NULL;
    struct arena *head = *current;
    size_t arena_size;

    if (likely(head != NULL)) {
        arena_size = head->size;
    } else {
        arena_size = ARENA_DEFAULT_SIZE;
    }

    if (unlikely(align_to(size, align) > arena_size)) {
        // The specified size is larger than the default arena size
        arena_size = size;
    } else {
        // Try to reserve block in the current arena
        ptr = arena_alloc(head, size, align);
        if (likely(ptr != NULL)) {
            return ptr;
        }
    }

    // We could not fit the block in the current arena
    // Allocate a new arena with the same size as the previous arena
    head = arena_list_add(list, arena_size, align);
    if (unlikely(head == NULL)) {
        return NULL;
    }
    
    ptr = arena_alloc(head, size, align);
    *current = head;
    return ptr;
}


/*
 * Identical to arena_alloc_dynamic, except that the allocated
 * memory is guaranteed to be initialized to zero.
 */
static inline
void * arena_alloc_dynamic_zeroed(struct arena_list *list, struct arena **current, size_t size, size_t align)
{
    void *ptr = arena_alloc_dynamic(list, current, size, align);
    memset(ptr, 0, size);
    return ptr;
}


#ifdef __cplusplus
}
#endif
#endif
