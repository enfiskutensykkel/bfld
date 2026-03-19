#ifndef BFLD_UTILS_ATOMIC_ARENA_ALLOCATOR_H
#define BFLD_UTILS_ATOMIC_ARENA_ALLOCATOR_H
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


#define PAGE_SIZE               4096
#define REGION_SIZE_DEFAULT     (2ULL << 20)    // default region size of 2 MB (used if arena's min_size is 0)
#define REGION_SIZE_MAX         (16ULL << 30)   // 16 GB ought to be enough for everyone


struct region;


/*
 * Lock-free, region-based memory allocation - "Arena allocation"
 *
 * Allocates memory in larger regions and provides a mechanism for lock-free,
 * bump allocation within a region. 
 *
 * By Using an arena allocator, tasks can store objects that have a similiar 
 * lifetimes in the same memory region rather than manually tracking memory 
 * manually with malloc/free. All memory regions are deallocated once with 
 * a single call to arena_destroy.
 *
 * This arena allocator dynamically adds additional regions when needed,
 * and uses a lock free bump allocation within a region to reserve blocks. 
 * This makes allocation very fast and makes extending memory trivial 
 * compared to realloc. The downside of this is that, once allocated, 
 * all objects live until the arena is destroyed (and all memory is deallocated). 
 */
struct arena
{
    struct region * _Atomic head;   // pointer to the current memory region 
    atomic_int spinlock;            // protect head manipulation with a spinlock
    size_t region_size;             // minimum region size for each region allocated by the arena
    size_t align;                   // default alignment for object allocation within a region
};


/*
 * A nemory region allocated by the arena allocator.
 */
struct region
{
    struct region * _Atomic next;   // pointer to the next region (linked list)
    uint8_t *data;                  // pointer to the actual memory
    size_t size;                    // total memory size of the region
    size_t _Atomic used;            // number of bytes currently used in the region
};


/*
 * Get the default alignment for arena allocations.
 */
static inline
size_t arena_align(const struct arena *arena)
{
    if (arena->align == 0) {
        return 1;
    }
    return arena->align;
}


/*
 * Get the region size for the arena.
 */
static inline
size_t arena_region_size(const struct arena *arena)
{
    size_t align = arena_align(arena);
    if (arena->region_size == 0) {
        return align_to(REGION_SIZE_DEFAULT, align);
    }

    return align_to(arena->region_size, align);
}


/*
 * Initialize the arena allocator with a given region size and alignment.
 */
static inline
void arena_init(struct arena *arena, size_t region_size, size_t align)
{
    align = align_roundup(align);
    region_size = align_to(region_size, align);
    if (region_size == 0) {
        region_size = REGION_SIZE_DEFAULT;
    }

    atomic_init(&arena->head, NULL);
    atomic_init(&arena->spinlock, 0);
    arena->region_size = region_size;
    arena->align = align;
}



static inline
bool arena_try_acquire_lock(struct arena *arena)
{
    int locked = 0;
    if (atomic_compare_exchange_strong_explicit(&arena->spinlock, &locked, 1,
                                                memory_order_release,
                                                memory_order_relaxed)) {
        return true;
    }

    // We failed to take the lock - yield to avoid thundering herd
#if defined(__x86_64__) || defined(__i386__)
    __asm__ __volatile__("pause");
#elif defined(__aarch64__)
    __asm__ __volatile__("yield");
#else
    thrd_yield();
#endif

    return false;
}


/*
 * Helper function to take the arena spin lock.
 */
static inline
void arena_acquire_lock(struct arena *arena)
{
    while (!arena_try_acquire_lock(arena));
}


/*
 * Helper function to release the arena spin lock.
 */
static inline
void arena_release_lock(struct arena *arena)
{
    atomic_store_explicit(&arena->spinlock, 0, memory_order_release);
}


/*
 * Helper function to allocate a memory region.
 */
struct region * region_alloc(size_t size);


/*
 * Helper function to release a memory region.
 */
void region_free(struct region *region);


/*
 * Destroy the arena allocator and free all memory regions allocated by it.
 * This must only be called once all previously allocated memory is no longer
 * in use.
 */
static inline
void arena_destroy(struct arena *arena)
{
    arena_acquire_lock(arena);
    struct region *head = atomic_exchange_explicit(&arena->head, NULL, memory_order_acquire);
    arena_release_lock(arena);

    while (head != NULL) {
        struct region *next = atomic_load_explicit(&head->next, memory_order_relaxed);
        region_free(head);
        head = next;
    }
}


/*
 * Helper function to bump a region, reserving a block of the specified size.
 * Returns a pointer to a reserved memory block, or NULL if the region is full.
 */
static inline
void * region_bump(struct region *region, size_t size, size_t align)
{
    size_t used, offset;
    uintptr_t base, addr;

    if (unlikely(region == NULL)) {
        return NULL;
    }

    base = (uintptr_t) region->data;
    used = atomic_load_explicit(&region->used, memory_order_acquire);

    do {
        addr = align_to(base + used, align);
        offset = (size_t) (addr - base);

        if (atomic_compare_exchange_weak_explicit(&region->used, &used, used + size,
                                                  memory_order_release, 
                                                  memory_order_acquire)) {
            VALGRIND_MAKE_MEM_UNDEFINED(&region->data[offset], size);
            return &region->data[offset];
        }
    } while (likely(offset + size <= region->size));

    return NULL;
}


/*
 * Allocate a memory block from a region.
 *
 * Reserves at least size bytes in a memory region with the specified alignment,
 * and returns a pointer to the allocated memory. 
 *
 * Note that the allocated memory is not initialised, and the specified
 * alignment must be a power of two.
 */
static inline
void * arena_alloc_cached(struct arena *arena, struct region **cache, size_t size, size_t align)
{
    void *ptr = NULL;
    struct region *head = *cache;

    if (align < arena_align(arena)) {
        align = arena_align(arena);
    }

    if (unlikely(size > arena_region_size(arena))) {
        // The specified size is larger than this arena's region size
        // We need to create a special oversized region for this allocation

        struct region *region = region_alloc(align_to(size, PAGE_SIZE));
        if (unlikely(region == NULL)) {
            return NULL;
        }

        ptr = region_bump(region, size, align);

        // Insert our oversized region
        arena_acquire_lock(arena);
        for (;;) {
            if (likely(head != NULL)) {
                struct region *next = atomic_load_explicit(&head->next, memory_order_relaxed);
                atomic_store_explicit(&region->next, next, memory_order_relaxed);
                atomic_store_explicit(&head->next, region, memory_order_release);
                break;
            } else {
                if (atomic_compare_exchange_strong_explicit(&arena->head, &head, region, 
                                                            memory_order_release,
                                                            memory_order_acquire)) {
                    break;
                }
            }
        }
        arena_release_lock(arena);
        *cache = head;
        return ptr;
    }

    for (;;) {
        // Try to reserve chunk in the current region
        ptr = region_bump(head, size, align);
        if (likely(ptr != NULL)) {
            *cache = head;
            return ptr;
        }

        // FIXME
        head = atomic_load_explicit(&arena->head, memory_order_acquire);

        // We could not fit the chunk in the current region
        // Try to insert a new region
        if (arena_try_acquire_lock(arena)) {


            // Allocate a new region 
            region = region_alloc(arena_region_size(arena));
            if (unlikely(region == NULL)) {
                arena_release_lock(arena);
                return NULL;
            }

            ptr = region_bump(region, size, align);
            atomic_store_explicit(&region->next, head, memory_order_relaxed);
            atomic_store_explicit(&arena->head, region, memory_order_release);
            arena_release_lock(arena);

            *cache = region;
            return ptr;
            
        } else {
            head = atomic_load_explicit(&arena->head, memory_order_acquire);
        }
    } 
}



/*
 *
 */
static inline
void * arena_alloc(struct arena *arena, size_t size, size_t align)
{
    struct region *head = atomic_load_explicit(&arena->head, memory_order_acquire);
    return arena_alloc_cached(arena, &head, size, align);
}


#ifdef __cplusplus
}
#endif
#endif
