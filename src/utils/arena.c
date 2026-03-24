#include "arena.h"
#include "align.h"
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <signal.h>
#include <assert.h>
//#include <numaif.h>

#if defined(HAS_VALGRIND) && !defined(NDEBUG)
#include <valgrind/memcheck.h>
#else
#define VALGRIND_MALLOCLIKE_BLOCK(addr, size, redzone_bytes, is_zeroed) (void) 0
#define VALGRIND_FREELIKE_BLOCK(addr, redzone_bytes) (void) 0
#define VALGRIND_MAKE_MEM_NOACCESS(addr, len) (void) 0
#define VALGRIND_MAKE_MEM_UNDEFINED(addr, len) (void) 0
#endif


struct arena * arena_list_add(struct arena_list *list, size_t size)
{
    if (size > SIZE_MAX - PAGE_SIZE) {
        return NULL;
    }

    size = align_to(size, PAGE_SIZE);

    struct arena *arena = aligned_alloc(sizeof(struct arena), CACHELINE_SIZE);
    if (arena == NULL) {
        return NULL;
    }

    void *memory = mmap(NULL, size, PROT_READ | PROT_WRITE, 
                        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (memory == MAP_FAILED) {
        free(arena);
        return NULL;
    }

    // The following needs -lnuma:
    // int cpu, node;
    // getcpu(&cpu, node);
    // mask = (1UL << node);
    // mbind(memory, size, MPOL_PREFERRED, &mask, sizeof(mask) * 8, 0)

#ifndef NDEBUG
    VALGRIND_MALLOCLIKE_BLOCK(memory, size, 0, 1);
    VALGRIND_MAKE_MEM_UNDEFINED(memory, size);
#endif

    atomic_init(&arena->used, 0);
    arena->size = size;
    arena->data = memory;
    atomic_init(&arena->next, NULL);

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
        mprotect(head->data, head->size, PROT_NONE);
        madvise(head->data, head->size, MADV_DONTNEED);
        VALGRIND_FREELIKE_BLOCK(head->data, 0);
#endif

        munmap(head->data, head->size);
        free(head);

        head = next;
    }
}
