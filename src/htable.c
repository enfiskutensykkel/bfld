#include "htable.h"
#include "align.h"
#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/mman.h>


#if defined(HAS_VALGRIND) && !defined(NDEBUG)
#include <valgrind/memcheck.h>
#else
#define VALGRIND_MALLOCLIKE_BLOCK(addr, size, redzone_bytes, is_zeroed) (void) 0
#define VALGRIND_FREELIKE_BLOCK(addr, redzone_bytes) (void) 0
#define VALGRIND_MAKE_MEM_NOACCESS(addr, len) (void) 0
#define VALGRIND_MAKE_MEM_UNDEFINED(addr, len) (void) 0
#endif


void htable_init(struct htable *ht, size_t capacity)
{
    ht->capacity = 0;
    ht->slots = NULL;
    atomic_init(&ht->size, 0);

    size_t real_cap = align_roundup(capacity);
    if (real_cap == 0 || real_cap > SIZE_MAX / sizeof(struct htable_node)) {
        return;
    }

    size_t size = real_cap * sizeof(struct htable_node);

    int flags = MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE;

    void *slotmem = mmap(NULL, size, PROT_READ | PROT_WRITE, flags, -1, 0);
    if (slotmem == MAP_FAILED) {
        return;
    }

    madvise(slotmem, size, MADV_HUGEPAGE);
    madvise(slotmem, size, MADV_RANDOM);
    madvise(slotmem, size, MADV_WILLNEED);

#ifndef NDEBUG
    VALGRIND_MALLOCLIKE_BLOCK(slotmem, size, 0, 1);
    VALGRIND_MAKE_MEM_DEFINED(slotmem, size);
#endif

    ht->slots = slotmem;
    ht->capacity = real_cap;
}


void htable_free(struct htable *ht)
{
    atomic_store_explicit(&ht->size, 0, memory_order_relaxed);

    size_t size = ht->capacity * sizeof(struct htable_node);

#ifndef NDEBUG
    madvise(ht->slots, size, MADV_DONTNEED);
    VALGRIND_FREELIKE_BLOCK(ht->slots, 0);
#endif

    munmap(ht->slots, size);

    ht->slots = NULL;
    ht->capacity = 0;
}
