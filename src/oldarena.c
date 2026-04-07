#include "cdefs.h"
#include "arena.h"
#include "align.h"
#include "valgrind.h"
#include <stddef.h>
#include <stdint.h>
#include <sys/mman.h>
#include <signal.h>
#include <assert.h>
#include <stdlib.h>

/*
 * Helper function go get the system page size at runtime.
 */
extern size_t get_page_size(void);

/*
 * Helper function to get the cache line size at runtime.
 */
extern size_t get_cache_line_size(void);


// Not sure if we need the following, but
//#include <numaif.h>
// link with -lnuma:
// int cpu, node;
// getcpu(&cpu, node);
// mask = (1UL << node);
// mbind(memory, size, MPOL_PREFERRED, &mask, sizeof(mask) * 8, 0)


struct arena * arena_list_add(struct arena_list *list, size_t size, size_t align)
{
    align = align_roundup(align);
    size_t page_size = get_page_size();
    if (align < page_size) {
        align = page_size;
    }

    if (size > SIZE_MAX - align) {
        return NULL;
    }

    size = align_to(size, align);

    size_t cacheline = get_cache_line_size();

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

    // FIXME: use MAP_HUGE_TLB and MAP_HUGE_2MB ?
    int flags = MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE;

    void *memory = mmap(NULL, size, PROT_READ | PROT_WRITE, flags, -1, 0);
    if (memory == MAP_FAILED) {
        free(arena);
        return NULL;
    }

#ifdef HAS_MADVISE
    madvise(memory, size, MADV_HUGEPAGE);
    madvise(memory, size, MADV_SEQUENTIAL);
#endif

#ifndef NDEBUG
    VALGRIND_MALLOCLIKE_BLOCK(memory, size, 0, 1);
    VALGRIND_MAKE_MEM_UNDEFINED(memory, size);
#endif

    atomic_init(&arena->used, 0);
    arena->size = size;
    arena->data = memory;
    atomic_init(&arena->next, NULL);

    // Make sure the next allocation starts on the specified alignment
    uintptr_t base = (uintptr_t) arena->data;
    uintptr_t addr = align_to(base, align);
    size_t offset = (size_t) (addr - base);
    atomic_store_explicit(&arena->used, offset, memory_order_relaxed);

    // Insert arena into arena list
    struct arena *head = atomic_load_explicit(&list->head, memory_order_acquire);

    do {
        atomic_store_explicit(&arena->next, head, memory_order_relaxed);
    } while (!atomic_compare_exchange_weak_explicit(&list->head, &head, arena,
                                                    memory_order_release,
                                                    memory_order_acquire));

    return arena;
}


void arena_list_free(struct arena_list *list)
{
    struct arena *head = atomic_exchange_explicit(&list->head, NULL, memory_order_acquire);
    
    while (head != NULL) {
        struct arena *next = atomic_load_explicit(&head->next, memory_order_relaxed);

#ifndef NDEBUG
        VALGRIND_FREELIKE_BLOCK(head->data, 0);
#endif

        munmap(head->data, head->size);
        free(head);

        head = next;
    }
}
