#include "cdefs.h"
#include "arena.h"
#include "align.h"
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/mman.h>
#include <valgrind.h>


#if !defined(NDEBUG) && defined(HAS_VALGRIND)
#include <valgrind/memcheck.h>
#else
#define VALGRIND_MALLOCLIKE_BLOCK(addr, size, redzone_bytes, is_zeroed) (void) 0
#define VALGRIND_FREELIKE_BLOCK(addr, redzone_bytes) (void) 0
#define VALGRIND_MAKE_MEM_NOACCESS(addr, len) (void) 0
#define VALGRIND_MAKE_MEM_UNDEFINED(addr, len) (void) 0
#define VALGRIND_MAKE_MEM_DEFINED(addr, len) (void) 0
#endif


//#include <numaif.h>
// link with -lnuma:
// int cpu, node;
// getcpu(&cpu, node);
// mask = (1UL << node);
// mbind(memory, size, MPOL_PREFERRED, &mask, sizeof(mask) * 8, 0)


/*
 * Helper function go get the system page size at runtime.
 */
extern size_t get_page_size(void);

/*
 * Helper function to get the cache line size at runtime.
 */
extern size_t get_cache_line_size(void);


struct arena * arena_create(size_t capacity)
{
    size_t page = get_page_size();
    size_t cacheline = get_cache_line_size();

    size_t size = align_to(capacity, page);

    struct arena *arena = NULL;

#if HAS_ALIGNED_ALLOC
    arena = aligned_alloc(cacheline, align_to(sizeof(struct arena), cacheline));
#elif HAS_POSIX_MEMALIGN
    if (posix_memalign((void**) &arena, cacheline, align_to(sizeof(struct arena), cacheline)) != 0) {
        arena = NULL;
    }
#else
    arena = malloc(align_to(sizeof(struct arena), cacheline));
#endif
    
    if (arena == NULL) {
        return NULL;
    }

    int flags = MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE;
    void *memory = mmap(NULL, size, PROT_READ | PROT_WRITE, flags, -1, 0);
    if (memory == MAP_FAILED) {
        free(arena);
        return NULL;
    }

#if HAS_MADVISE
    madvise(memory, size, MADV_HUGEPAGE);
    madvise(memory, size, MADV_RANDOM);
#endif

    VALGRIND_MALLOCLIKE_BLOCK(memory, size, 0, 1);
    VALGRIND_MAKE_MEM_UNDEFINED(memory, size);

    atomic_init(&arena->used, 0);
    arena->capacity = size;
    arena->base = (uint8_t*) memory;
    atomic_init(&arena->next, NULL);
    atomic_init(&arena->refcnt, 1);
    return arena;
}


void arena_destroy(struct arena *arena)
{
    VALGRIND_FREELIKE_BLOCK(arena->base, arena->capacity);
    munmap(arena->base, arena->capacity);
    free(arena);
}
