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

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#elif defined(__linux__) || defined(__unix__) || defined(__unix) || (defined(__APPLE__) && defined(__MACH__))
#include <unistd.h>
#if defined(__APPLE__)
#include <sys/sysctl.h>
#endif
#endif


static size_t get_page_size(void)
{
#if defined(_WIN32) || defined(_WIN64)
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    return (size_t) si.dwPageSize;
#elif defined(_SC_PAGESIZE)
    return (size_t) sysconf(_SC_PAGESIZE);
#elif defined(_SC_PAGE_SIZE)
    return (size_t) sysconf(_SC_PAGE_SIZE);
#else
    // Default fallback to 4096
    return 4096;
#endif
}

size_t get_cache_line_size(void)
{
#if defined(_SC_LEVEL1_DCACHE_LINESIZE)
    return (size_t) sysconf(_SC_LEVEL1_DCACHE_LINESIZE);
#elif defined(__APPLE__)
    size_t line_size = 0;
    size_t length = sizeof(line_size);
    sysctlbyname("hw.cachelinesize", &line_size, &length, NULL, 0);
    return line_size;
#elif defined(_WIN32) || defined(_WIN64)
    // FIXME: use GetLogicalProcessorInformation
    return 64;
#else
    return 64; // default fallback for modern x86_64/ARM
#endif
}


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


// Not sure if we need the following, but
//#include <numaif.h>
// link with -lnuma:
// int cpu, node;
// getcpu(&cpu, node);
// mask = (1UL << node);
// mbind(memory, size, MPOL_PREFERRED, &mask, sizeof(mask) * 8, 0)

void arena_list_free(struct arena_list *list)
{
    struct arena *head = atomic_exchange_explicit(&list->head, NULL, memory_order_acquire);
    
    while (head != NULL) {
        struct arena *next = atomic_load_explicit(&head->next, memory_order_relaxed);

#ifndef NDEBUG
        mprotect(head->data, head->size, PROT_NONE);
#ifdef HAS_MADVISE
        madvise(head->data, head->size, MADV_DONTNEED);
#endif
        VALGRIND_FREELIKE_BLOCK(head->data, 0);
#endif

        munmap(head->data, head->size);
        free(head);

        head = next;
    }
}
