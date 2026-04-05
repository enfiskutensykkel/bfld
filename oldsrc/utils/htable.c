#include "htable.h"
#include "align.h"
#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>


void htable_init(struct htable *ht, size_t capacity)
{
    ht->capacity = 0;
    ht->slots = NULL;
    atomic_init(&ht->size, 0);

    size_t real_cap = align_roundup(capacity);
    if (real_cap == 0 || real_cap > SIZE_MAX / sizeof(struct htable_slot)) {
        return;
    }

    ht->slots = aligned_alloc(64, sizeof(struct htable_slot) * real_cap);
    if (ht->slots == NULL) {
        return;
    }

    memset(ht->slots, 0, real_cap * sizeof(struct htable_slot));

    ht->capacity = real_cap;
}


void htable_free(struct htable *ht)
{
    atomic_store_explicit(&ht->size, 0, memory_order_relaxed);
    free(ht->slots);
    ht->slots = NULL;
    ht->capacity = 0;
}
