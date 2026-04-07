#ifndef BFLD_ARENA_ALLOCATOR_H
#define BFLD_ARENA_ALLOCATOR_H

#include "cdefs.h"
#include "spinlock.h"
#include <stddef.h>
#include <stdbool.h>


/*
 * Concurrent arena allocator - multi-threaded bump allocator.
 *
 * Allocates a memory region in advance using mmap() with MAP_NORESERVE,
 * and provides a mechanism for lock free bump allocation within that region.
 * This allows very fast and contention free allocations for objects that
 * have similar lifetimes, rather than tracking them manually with malloc/free.
 *
 * Note that even though this arena allocator supports atomic bump allocation,
 * it is still suboptimal to have contention for a single, shared arena.
 * Ideally use a thread-local arena per thread in order to avoid contention.
 */
struct shared_arena
{
    alignas(64) _Atomic(size_t) capacity;  // guaranteed capacity of the arena
    unsigned char* base;    // base pointer of the arena memory region
    size_t grow;            // the growth increment of the arena memory region
    char pad0[64 - sizeof(_Atomic(size_t)) - sizeof(size_t) - sizeof(unsigned char*)];
    struct spinlock lock;   // ensure that calls to mprotect happens exclusively
    char pad1[64 - sizeof(struct spinlock)];
    alignas(64) _Atomic(size_t) used;   // number of bytes currently used
};



/*
 * Initialize the concurrent arena allocator.
 */
void shared_arena_init(struct shared_arena *arena);


/*
 * Helper function to get the number of bytes currently
 * used in the concurrent arena.
 */
static inline
size_t shared_arena_used(const struct shared_arena *arena)
{
    return atomic_load_explicit(&arena->used, memory_order_relaxed);
}


/*
 * Ensure that the underlying memory region is large enough
 * for (at least) the specified capacity.
 */
bool shared_arena_reserve(struct shared_arena *arena,
                              size_t capacity);


/*
 * Alloate a memory block of (at least) the specified size
 * in the arena, starting at the specified alignment.
 *
 * Tries to reserve a block within the arena and returns a non-NULL
 * pointer to the reserved memory block. The pointer is aligned to
 * the specified alignment (which must be a power of two).
 *
 * On success, this function returns a non-NULL pointer to the
 * reserved memory block. On failure, this function returns NULL.
 *
 * This function is thread-safe, allowing two or more threads/tasks
 * to use the same arena under contention. However, the user should
 * be aware of contention-related performance implications, such as
 * cache line contention.
 */
void * shared_arena_alloc(struct shared_arena *arena, 
                              size_t size, 
                              size_t alignment);


/*
 * Reset the arena and set the bump allocator back to zero.
 *
 * Note that this should only be called once it is 
 * guaranteed that memory is no longer in used.
 */
static inline
void shared_arena_reset(struct shared_arena *arena)
{
    atomic_store_explicit(&arena->used, 0, memory_order_release);
}


/*
 * The same as shared_arena_alloc but the allocated 
 * memory is initialized to zero.
 */
void * shared_arena_alloc_zeroed(struct shared_arena *arena,
                                     size_t size,
                                     size_t alignment);


/*
 * Tear down the arena and release the underlying memory region.
 * Note that this is not thread-safe, meaning that this function 
 * must only be called once the arena should no longer be used.
 */
void shared_arena_free(struct shared_arena *arena);


/*
 * Thread-local arena allocator - single-threaded bump allocator.
 *
 * Allocates a memory region in advance using mmap() with MAP_NORESERVE,
 * and provides a mechanism for bump allocation within that region.
 * This allows very fast and contention free allocations for objects that
 * have similar lifetimes, rather than tracking them manually with malloc/free.
 */
struct local_arena
{
    size_t capacity;        // guaranteed capacity of the arena
    unsigned char *base;    // base pointer of the arena memory region
    size_t grow;            // the growth increment of the arena memory region
    size_t used;            // number of bytes currently used
};


/*
 * Initialize the thread-local arena allocator.
 */
void local_arena_init(struct local_arena *arena);


/*
 * Ensure that the underlying memory region is large enough
 * for (at least) the specified capacity.
 */
bool local_arena_reserve(struct local_arena *arena,
                               size_t capacity);
                       

/*
 * Alloate a memory block of (at least) the specified size
 * in the arena, starting at the specified alignment.
 *
 * Tries to reserve a block within the arena and returns a non-NULL
 * pointer to the reserved memory block. The pointer is aligned to
 * the specified alignment (which must be a power of two).
 *
 * On success, this function returns a non-NULL pointer to the
 * reserved memory block. On failure, this function returns NULL.
 */
void * local_arena_alloc(struct local_arena *arena, 
                               size_t size, 
                               size_t alignment);


/*
 * The same as local_arena_alloc but the allocated 
 * memory is initialized to zero.
 */
void * local_arena_alloc_zeroed(struct local_arena *arena, 
                                      size_t size, size_t alignment);


/*
 * Reset the arena and set the bump allocator back to zero.
 *
 * Note that this should only be called once it is 
 * guaranteed that memory is no longer in used.
 */
static inline
void local_arena_reset(struct local_arena *arena)
{
    arena->used = 0;
}


/*
 * Tear down the arena and release the underlying memory region.
 */
void local_arena_free(struct local_arena *arena);

#endif
