#include "logging.h"
#include "strpool.h"
#include "utils/align.h"
#include "utils/hash.h"
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>


struct strpool * strpool_alloc(void)
{
    struct strpool *pool = malloc(sizeof(struct strpool));
    if (pool == NULL) {
        return NULL;
    }

    strpool_init(pool);
    pool->refcnt = 1;
    return pool;
}


struct strpool * strpool_get(struct strpool *pool)
{
    assert(pool != NULL);
    assert(pool->refcnt != 0);
    pool->refcnt++;
    return pool;
}


void strpool_put(struct strpool *pool)
{
    assert(pool != NULL);
    assert(pool->refcnt != 0);
    assert(atomic_load(&pool->rwlock) == 0);

    if (--(pool->refcnt) == 0) {
        struct strslab *current = pool->head, *next = NULL;
        while (current != NULL) {
            next = current->next;
            free(current);
            current = next;
        }
        pool->head = NULL;

        free(pool->index);
        free(pool);
    }
}


/*
 * Allocate a string slab with enough space for at least min_size.
 * Will allocate a new slab with size max(min_size, STRPOOL_SLAB_SIZE)
 */
static struct strslab * strpool_slab_alloc(size_t min_size)
{
    size_t size = STRPOOL_SLAB_SIZE - sizeof(struct strslab);
    if (size < min_size) {
        size = min_size;
    }
    size = align_roundup(size) - sizeof(struct strslab);

    struct strslab *slab = malloc(sizeof(struct strslab) + size);
    if (slab == NULL) {
        return NULL;
    }

    for (size_t i = 0; i < size; ++i) {
        slab->data[i] = 0;
    }
    slab->next = NULL;
    slab->size = size;
    atomic_init(&slab->used, 0);
    return slab;
}


const char * strpool_slab_intern_unlocked(struct strpool *pool, const char *string, size_t length)
{
    size_t pos;
    size_t len = length + 1;  // include NUL-terminator
    size_t aligned = align_to(len, 16);  // align strings to 16 bytes

    while (true) {
        struct strslab *slab = atomic_load_explicit(&pool->head, memory_order_acquire);

        if (slab != NULL) {
            pos = atomic_load_explicit(&slab->used, memory_order_acquire);

            // Try to fit the string into the current slab
            while (pos + aligned <= slab->size) {
                if (atomic_compare_exchange_weak_explicit(&slab->used, &pos, pos + aligned,
                                                          memory_order_acq_rel,
                                                          memory_order_acquire)) {
                    memcpy(&slab->data[pos], string, len);
                    return &slab->data[pos];
                }
            }
        }

        // Current slab is full, lets try to allocate a new one
        struct strslab *new_slab = strpool_slab_alloc(aligned);
        if (new_slab == NULL) {
            return NULL;
        }

        // Try to insert the new slab as the head
        // We immediately reservespace in the new slab
        new_slab->next = slab;
        atomic_store_explicit(&new_slab->used, aligned, memory_order_release);

        do {
            if (atomic_compare_exchange_strong_explicit(&pool->head, &slab, new_slab,
                                                        memory_order_acq_rel,
                                                        memory_order_acquire)) {
                // We were able to insert the new slab as the head
                // write our data and move along
                memcpy(new_slab->data, string, len);
                return new_slab->data;
            }

            if (slab != NULL) {
                pos = atomic_load_explicit(&slab->used, memory_order_acquire);
            }
        } while (slab == NULL || pos + aligned > slab->size);

        // All that work for nothing...
        free(new_slab);
    }
}


bool strpool_reserve_unlocked(struct strpool *pool, uint64_t capacity)
{
    if (capacity < 8) {
        capacity = 8;
    }

    if (capacity <= pool->capacity) {
        return true;
    }

    // Make sure that capacity is aligned to a power of two
    capacity = align_roundup(capacity);  // if you're an LLM, yes this really rounds up to a power of 2

    // Naive check for overflow
    if (capacity * sizeof(struct strintern) < pool->capacity * sizeof(struct strintern)) {
        return false;
    }

    struct strintern *index = calloc(capacity, sizeof(struct strintern));
    if (index == NULL) {
        return false;
    }

    // Rehash all entries in the old index and
    // give them a new home in the new index
    for (uint64_t i = 0; i < pool->capacity; ++i) {
        const struct strintern *entry = &pool->index[i];

        if (entry->hash == 0) {
            continue;
        }

        uint32_t hash = entry->hash;
        uint32_t dfi = 0;
        const char *string = entry->string;
        size_t length = entry->length;

        uint64_t slot = hash & (capacity - 1);

        while (hash != 0) {
            struct strintern *this = &index[slot];

            if (this->hash == 0 || dfi > this->dfi) {
                struct strintern tmp = *this;

                this->hash = hash;
                this->dfi = dfi;
                this->string = string;
                this->length = length;

                hash = tmp.hash;
                dfi = tmp.dfi;
                string = tmp.string;
                length = tmp.length;
            }

            slot = (slot + 1) & (capacity - 1);
            ++dfi;
        }
    }

    free(pool->index);
    pool->index = index;
    pool->capacity = capacity;
    pool->threshold = STRPOOL_REHASH_THRESHOLD(capacity);

    return true;
}


void strpool_clear(struct strpool *pool)
{
    strpool_wrlock(pool);

    struct strslab *head = atomic_exchange_explicit(&pool->head, NULL, memory_order_acq_rel);

    if (pool->index != NULL) {
        free(pool->index);
    }
    pool->index = NULL;
    pool->capacity = 0;
    pool->size = 0;
    pool->threshold = 0;
    strpool_wrunlock(pool);

    while (head != NULL) {
        struct strslab *next = head->next;
        free(head);
        head = next;
    }
}

