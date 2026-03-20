#ifndef BFLD_UTILS_ARENA_ALLOCATOR_H
#define BFLD_UTILS_ARENA_ALLOCATOR_H
#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdalign.h>
#include <stdatomic.h>
#include "align.h"
#include "cdefs.h"

#if defined(HAS_VALGRIND) && !defined(NDEBUG)
#include <valgrind/memcheck.h>
#else
#define VALGRIND_MAKE_MEM_UNDEFINED(addr, len) (void) 0
#endif


#define PAGE_SIZE   4096            // system page size


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
    struct arena * _Atomic next;    // pointer to the next memory region
    size_t _Atomic used;            // number of bytes currently used
    size_t size;                    // total size of the memory region
    uint8_t *data;                  // pointer to the actual memory
};


/*
 * Track all arenas in a list, allowing arenas to be destroyed and
 * memory to be freed once at the end of the program.
 */
struct arena_list
{
    struct arena * _Atomic head;
    size_t size;
};


/*
 * Get the default arena size.
 */
static inline
size_t arena_size(const struct arena_list *list)
{
    if (list->size == 0) {
        return 2ULL << 20;  // default size = 2 MB
    }

    return list->size;
}


/*
 * Initialize the arena list with a default arena size.
 */
static inline
void arena_list_init(struct arena_list *list, size_t size)
{
    atomic_init(&list->head, NULL);
    list->size = align_to(size, PAGE_SIZE);
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
 * This function is thread-safe, allowing two or more threads/tasks
 * to use the same arena under contention. However, the user should
 * beware of problems related to contention, particularly cache 
 * contention.
 */
static inline
void * arena_alloc_block_threadsafe(struct arena *arena, size_t size, size_t align)
{
    size_t used, offset;
    uintptr_t base, addr;

    if (unlikely(arena == NULL)) {
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
 * Allocate a memory block of (at least) the specified size within an arena,
 * using the specified alignment.
 *
 * Tries to reserve a block within the specified arena. On success,
 * this function returns a non-NULL pointer to the reserved memory block.
 * The pointer is aligned to the specified alignment (which must be 
 * a power of two). On failure, this function returns NULL.
 *
 * Unlike arena_alloc_block_threadsafe, this function should NOT be
 * used when the arena is shared between multiple threads.
 */
static inline
void * arena_alloc_block(struct arena *arena, size_t size, size_t align)
{
    if (unlikely(arena == NULL)) {
        return NULL;
    }

    uintptr_t base = (uintptr_t) arena->data;
    size_t used = atomic_load_explicit(&arena->used, memory_order_relaxed);

    uintptr_t addr = align_to(base + used, align);
    size_t offset = (size_t) (addr - base);

    if (unlikely(offset + size > arena->size)) {
        return NULL;
    }

    atomic_store_explicit(&arena->used, offset + size, memory_order_relaxed);    
    VALGRIND_MAKE_MEM_UNDEFINED(&arena->data[offset], size);
    return &arena->data[offset];
}


/*
 * Create an arena of the specified size and allocate 
 * its underlying memory region.
 *
 * This also adds the created arena to the specified list.
 */
struct arena * arena_create(struct arena_list *list, size_t size);


/*
 * Destroy all arenas tracked in the specified list,
 * and free their underlying memory regions.
 */
void arena_destroy(struct arena_list *list);


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

    if (unlikely(size > arena_size(list))) {
        // The specified size is larger than the default arena size
        // We need to create an oversized arena for this allocation
        if (align < PAGE_SIZE) {
            align = PAGE_SIZE;
        }
        head = arena_create(list, align_to(size, align));
        if (unlikely(head == NULL)) {
            return NULL;
        }

        ptr = arena_alloc_block(head, size, align);
        return ptr;
    }

    // Try to reserve block in the current arena
    ptr = arena_alloc_block(head, size, align);
    if (likely(ptr != NULL)) {
        return ptr;
    }

    // We could not fit the block in the current arena
    head = arena_create(list, arena_size(list));
    if (unlikely(head == NULL)) {
        return NULL;
    }
    
    ptr = arena_alloc_block(head, size, align);
    *current = head;
    return ptr;
}


/*
 * Identical to arena_alloc_dynamic, except that the allocated
 * memory is guaranteed to be initialized to zero.
 */
#if defined(HAS_VALGRIND) && !defined(NDEBUG)
static inline
void * arena_alloc_dynamic_zeroed(struct arena_list *list, struct arena **current, size_t size, size_t align)
{
    void *ptr = arena_alloc_dynamic(list, current, size, align);
    VALGRIND_MAKE_MEM_DEFINED(ptr, size);
    return ptr;
}
#else
#define arena_alloc_dynamic_zeroed(list, current, size, align) arena_alloc_dynamic(list, current, size, align)
#endif


#ifdef __cplusplus
}
#endif
#endif
