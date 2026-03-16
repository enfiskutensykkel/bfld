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
        struct strslab *current = atomic_load(&pool->head), *next = NULL;
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
 */
static struct strslab * strpool_slab_alloc(size_t used)
{
    size_t size = STRING_POOL_MIN_SLAB_SIZE - sizeof(struct strslab);
    if (size < used) {
        size = used;
    }
    size = align_roundup(size) - sizeof(struct strslab);

    struct strslab *slab = malloc(sizeof(struct strslab) + size);
    if (slab == NULL) {
        return NULL;
    }

    slab->next = NULL;
    atomic_init(&slab->used, used);
    slab->size = size;
    return slab;
}


const char * strpool_slab_intern(struct strpool *pool, const char *string, size_t length)
{
    size_t pos;
    size_t len = length + 1;  // include NUL-terminator

    while (true) {
        strpool_rdlock(pool);
        struct strslab *slab = atomic_load(&pool->head);
        strpool_rdunlock(pool);

        if (slab != NULL) {
            pos = atomic_load(&slab->used);

            // Try to fit the string into the current slab
            while (pos + len <= slab->size) {
                if (atomic_compare_exchange_weak(&slab->used, &pos, pos + len)) {
                    memcpy(&slab->data[pos], string, len);
                    return &slab->data[pos];
                }
            }
        }

        // Current slab is full, we need to allocate a new one
        struct strslab *new_slab = strpool_slab_alloc(len);
        if (new_slab == NULL) {
            return NULL;
        }

        // FIXME
        strpool_wrlock(pool);
        new_slab->next = slab;
        if (pool->head == slab) {
            pool->head = slab;
            strpool_wrunlock(pool);
            memcpy(new_slab->data, string, len);
            return new_slab->data;
        }
        strpool_wrunlock(pool);
        free(new_slab);

#if 0
        memcpy(new_slab->data, string, len);  // safe because we don't export data before it is in the hash table
        atomic_thread_fence(memory_order_release);

        do {
            atomic_store_explicit(&new_slab->next, slab, memory_order_release);

            // Try to replace the pool's head slab with the new one
            if (atomic_compare_exchange_strong_explicit(&pool->head, &slab, new_slab,
                                                        memory_order_release,
                                                        memory_order_acquire)) {
                //memcpy(new_slab->data, string, len);  // safe because we don't export data before it is in the hash table

                return new_slab->data;
            }

            atomic_thread_fence(memory_order_acquire);
            pos = atomic_load_explicit(&slab->used, memory_order_acquire);
            size = atomic_load_explicit(&slab->size, memory_order_acquire);
        } while (pos + len > size);

        // We failed to insert our new slab as pool's head
        // Release the allocated slab and try all over again
        free(new_slab);
#endif
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
    pool->threshold = STRING_POOL_REHASH_THRESHOLD(capacity);

    return true;
}


void strpool_clear(struct strpool *pool)
{
    strpool_wrlock(pool);
    if (pool->index != NULL) {
        free(pool->index);
    }
    pool->index = NULL;
    pool->capacity = 0;
    pool->size = 0;
    pool->threshold = 0;

    struct strslab *current = atomic_load(&pool->head), *next = NULL;
    while (current != NULL) {
        next = current->next;
        free(current);
        current = next;
    }
    atomic_store(&pool->head, NULL);
    strpool_wrunlock(pool);
}

