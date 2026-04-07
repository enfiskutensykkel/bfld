#include "htable.h"
#include "align.h"
#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/mman.h>


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

#ifdef HAS_MADVISE
    madvise(slotmem, size, MADV_HUGEPAGE);
    madvise(slotmem, size, MADV_RANDOM);
    madvise(slotmem, size, MADV_WILLNEED);
#endif

    ht->slots = slotmem;
    ht->capacity = real_cap;
}


void htable_free(struct htable *ht)
{
    atomic_store_explicit(&ht->size, 0, memory_order_relaxed);

    size_t size = ht->capacity * sizeof(struct htable_node);

    munmap(ht->slots, size);

    ht->slots = NULL;
    ht->capacity = 0;
}
