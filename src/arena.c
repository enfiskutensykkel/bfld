#include "cdefs.h"
#include "arena.h"
#include "align.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <sys/mman.h>

#define ARENA_MAX_CAPACITY  (128ULL << 20)

#if !defined(NDEBUG) && defined(HAS_VALGRIND)
#include <valgrind/memcheck.h>
#else
#define VALGRIND_MALLOCLIKE_BLOCK(addr, size, redzone_bytes, is_zeroed) (void) 0
#define VALGRIND_FREELIKE_BLOCK(addr, redzone_bytes) (void) 0
#define VALGRIND_MAKE_MEM_NOACCESS(addr, len) (void) 0
#define VALGRIND_MAKE_MEM_UNDEFINED(addr, len) (void) 0
#define VALGRIND_MAKE_MEM_DEFINED(addr, len) (void) 0
#endif


extern size_t get_page_size(void);


bool shared_arena_reserve(struct shared_arena *arena, size_t required)
{
    if (unlikely(arena->base == NULL)) {
        return NULL;
    }

    required = align_to(required, arena->grow);
    if (unlikely(required > ARENA_MAX_CAPACITY)) {
        return false;
    }

    size_t capacity = atomic_load_explicit(&arena->capacity, memory_order_acquire);
    while (unlikely(required > capacity)) {

        if (spinlock_try_lock(&arena->lock)) {
            capacity = atomic_load_explicit(&arena->capacity, memory_order_acquire);
            if (likely(capacity >= required)) {
                spinlock_unlock(&arena->lock);
                return true;
            }

            void *ptr = (void*) (arena->base + capacity);
            size_t size = required - capacity;
            int status = mprotect(ptr, size, PROT_READ | PROT_WRITE);
            if (status != 0) {
                spinlock_unlock(&arena->lock);
                return false;
            }

#ifdef HAS_MADVISE
            madvise(ptr, size, MADV_WILLNEED);
            madvise(ptr, size, MADV_HUGEPAGE);
#else
            posix_madvise(ptr, size, POSIX_MADV_WILLNEED);
#endif
            atomic_store_explicit(&arena->capacity, required, memory_order_release);
            spinlock_unlock(&arena->lock);
            return true;
        }

        thread_pause();
        capacity = atomic_load_explicit(&arena->capacity, memory_order_acquire);
    }

    return true;
}


void * shared_arena_alloc(struct shared_arena *arena, 
                              size_t size, size_t align)
{
    size_t used, start, end;
    uintptr_t base, addr;

    assert(arena != NULL);

    if (unlikely(arena->base == NULL)) {
        return NULL;
    }

    base = (uintptr_t) arena->base;
    used = atomic_load_explicit(&arena->used, memory_order_acquire);

    for (;;) {
        addr = align_to(base + used, align);
        start = (size_t) (addr - base);
        
        // Check that we're not exceeding the limit
        if (unlikely(size > ARENA_MAX_CAPACITY  - start)) {
            return NULL;
        }

        end = start + size;

        // Try to reserve the block
        if (atomic_compare_exchange_weak_explicit(&arena->used, &used, end,
                                                  memory_order_release,
                                                  memory_order_acquire)) {
            break;
        }
    }

    // Check if we need to notify the OS that we need more memory
    if (unlikely(!shared_arena_reserve(arena, end))) {
        return NULL;
    }

#ifndef NDEBUG
    VALGRIND_MAKE_MEM_UNDEFINED((void*) (arena->base + start), size);
#endif
    return (void*) (arena->base + start);
}


void * shared_arena_alloc_zeroed(struct shared_arena *arena, 
                                     size_t size, size_t align)
{
    void *ptr = shared_arena_alloc(arena, size, align);
    if (ptr != NULL) {
        memset(ptr, 0, size);
    }
    return ptr;
}


void shared_arena_init(struct shared_arena *arena)
{
    size_t page_size = get_page_size();
    atomic_init(&arena->capacity, 0);
    arena->base = NULL;
    arena->grow = align_to(2ULL << 20, page_size);
    spinlock_init(&arena->lock);
    atomic_init(&arena->used, 0);

    void *region = (void*) mmap(NULL, ARENA_MAX_CAPACITY, /*PROT_NONE*/ PROT_READ | PROT_WRITE, 
                                MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
    if (region == MAP_FAILED) {
        return;
    }

    arena->capacity = ARENA_MAX_CAPACITY;
    arena->base = (unsigned char*) region;
}


void shared_arena_free(struct shared_arena *arena)
{
    assert(arena != NULL);

    if (arena->base != NULL) {
        size_t size = atomic_exchange_explicit(&arena->capacity, 0, memory_order_acq_rel);

#if HAS_MADVISE
        madvise(arena->base, size, MADV_DONTNEED);
#else
        posix_madvise(arena->base, size, POSIX_MADV_DONTNEED);
#endif

        mprotect(arena->base, size, PROT_NONE);

#ifndef NDEBUG
        VALGRIND_MAKE_MEM_NOACCESS(arena->base, size);
#endif

        atomic_store_explicit(&arena->used, 0, memory_order_relaxed);

        munmap(arena->base, ARENA_MAX_CAPACITY);
        arena->base = NULL;
    }
}

#include <stdio.h>
#include <errno.h>


void local_arena_init(struct local_arena *arena)
{
    size_t page_size = page_size;
    arena->capacity = 0;
    arena->base = NULL;
    arena->grow = align_to(2ULL << 20, page_size);
    arena->used = 0;

    void *region = (void*) mmap(NULL, ARENA_MAX_CAPACITY, PROT_NONE, 
                                MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
    if (region == MAP_FAILED) {
        fprintf(stderr, "%d\n", errno);
        return;
    }

    arena->base = (unsigned char*) region;
}


bool local_arena_reserve(struct local_arena *arena, size_t required)
{
    if (unlikely(arena->base == NULL)) {
        return NULL;
    }

    required = align_to(required, arena->grow);
    if (unlikely(required > ARENA_MAX_CAPACITY)) {
        return false;
    }

    if (unlikely(required > arena->capacity)) {
        void *ptr = (void*) (arena->base + arena->capacity);
        size_t size = required - arena->capacity;

        int status = mprotect(ptr, size, PROT_READ | PROT_WRITE);
        if (status != 0) {
            return false;
        }

#ifdef HAS_MADVISE
        madvise(ptr, size, MADV_WILLNEED);
        madvise(ptr, size, MADV_HUGEPAGE);
#else
        posix_madvise(ptr, size, POSIX_MADV_WILLNEED);
#endif
        arena->capacity = required;
    }
    return true;
}


void * local_arena_alloc(struct local_arena *arena, 
                         size_t size, size_t align)
{
    assert(arena != NULL);

    if (unlikely(arena->base == NULL)) {
        return NULL;
    }
    
    uintptr_t base = (uintptr_t) arena->base;
    size_t used = arena->used;

    uintptr_t addr = align_to(base + used, align);
    size_t start = (size_t) (addr - base);

    if (unlikely(size > ARENA_MAX_CAPACITY - start)) {
        return NULL;
    }

    size_t end = start + size;
    if (!local_arena_reserve(arena, end)) {
        return NULL;
    }

    arena->used = end;
#ifndef NDEBUG
    VALGRIND_MAKE_MEM_UNDEFINED((void*) (arena->base + start), size);
#endif
    return (void*) (arena->base + start);
}


void * local_arena_alloc_zeroed(struct local_arena *arena,
                                      size_t size, size_t align)
{
    void *ptr = local_arena_alloc(arena, size, align);
    if (ptr != NULL) {
        memset(ptr, 0, size);
    }
    return ptr;
}


void local_arena_free(struct local_arena *arena)
{
    if (arena->base != NULL) {
        size_t size = arena->capacity;
        arena->capacity = 0;

#if HAS_MADVISE
        madvise(arena->base, size, MADV_DONTNEED);
#else
        posix_madvise(arena->base, size, POSIX_MADV_DONTNEED);
#endif

        mprotect(arena->base, size, PROT_NONE);

#ifndef NDEBUG
        VALGRIND_MAKE_MEM_NOACCESS(arena->base, size);
#endif

        arena->used = 0;
        munmap(arena->base, ARENA_MAX_CAPACITY);
        arena->base = NULL;
    }
}
