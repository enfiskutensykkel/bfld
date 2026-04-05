#ifndef BFLD_CONCURRENT_HASH_TABLE_H
#define BFLD_CONCURRENT_HASH_TABLE_H
#ifdef __cplusplus
extern "C" {
#endif

#include <spinlock.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdatomic.h>
#include <stdalign.h>
#include "cdefs.h"
#include "align.h"
#include "spinlock.h"


struct htable_entry
{
    uint64_t hash;
    const void *key;
    size_t key_size;
    void *value;
};


struct htable_bucket
{
    struct rwlock lock;
    struct htable_entry *entries;
    size_t capacity;
    size_t size;
};


struct htable
{
    _Atomic(struct htable_bucket*) *buckets;
    atomic_size_t size;
    size_t capacity;
};



static inline
void htable_init(struct htable *ht, size_t capacity)
{
    capacity = align_roundup(capacity);
    ht->buckets = calloc(capacity, sizeof(_Atomic(struct htable_bucket*)));
    atomic_init(&ht->size, 0);
    ht->capacity = capacity;

}


static inline
void htable_free(struct htable *ht)
{
    for (size_t idx = 0; idx < ht->capacity; ++idx) {
        struct htable_bucket *bucket = atomic_exchange(&ht->buckets[idx], NULL);
        if (bucket != NULL) {
            free(bucket->entries);
            free(bucket);
        }
    }
}


static inline
size_t htable_size(const struct htable *ht)
{
    return atomic_load_explicit(&ht->size, memory_order_relaxed);
}


static inline
void * htable_get(const struct htable *ht, uint64_t hash, const void *key, size_t key_size)
{
    size_t bucket_idx = hash & (ht->capacity - 1);
    struct htable_bucket *bucket = atomic_load_explicit(&ht->buckets[bucket_idx], memory_order_acquire);

    if (likely(bucket != NULL)) {
        rwlock_read_lock(&bucket->lock);

        for (size_t entry_idx = 0, num_entries = bucket->size; entry_idx < num_entries; ++entry_idx) {
            const struct htable_entry *entry = &bucket->entries[entry_idx];

            if (likely(entry->hash == hash)) {
                if (entry->key_size == key_size &&
                        (entry->key == key || !memcmp(entry->key, key, key_size))) {
                    void *value = entry->value;
                    rwlock_read_unlock(&bucket->lock);
                    return value;
                }
            }
        }

        rwlock_read_unlock(&bucket->lock);
    }
    return NULL;
}

static inline
void * htable_put(struct htable *ht, uint64_t hash, const void *key, size_t key_size, void *value)
{
    size_t bucket_idx = hash & (ht->capacity - 1);
    struct htable_bucket *bucket = atomic_load_explicit(&ht->buckets[bucket_idx], memory_order_acquire);

    if (unlikely(bucket == NULL)) {
        struct htable_bucket *new_bucket = malloc(sizeof(struct htable_bucket));
        if (unlikely(new_bucket == NULL)) {
            return NULL;
        }

        struct htable_entry *entries = malloc(1 * sizeof(struct htable_entry));
        if (unlikely(entries == NULL)) {
            free(new_bucket);
            return NULL;
        }

        rwlock_init(&new_bucket->lock);
        new_bucket->capacity = 1;
        new_bucket->size = 0;
        new_bucket->entries = entries;

        if (likely(atomic_compare_exchange_strong_explicit(&ht->buckets[bucket_idx], &bucket, new_bucket,
                                                           memory_order_release, memory_order_acquire))) {
            bucket = new_bucket;
        } else {
            free(new_bucket->entries);
            free(new_bucket);
        }
    }

    rwlock_biased_write_lock(&bucket->lock);
    for (size_t entry_idx = 0, num_entries = bucket->size; entry_idx < num_entries; ++entry_idx) {
        const struct htable_entry *entry = &bucket->entries[entry_idx];
        if (likely(entry->hash == hash)) {
            if (entry->key_size == key_size &&
                    (entry->key == key || !memcmp(entry->key, key, key_size))) {
                value = entry->value;
                rwlock_write_unlock(&bucket->lock);
                return value;
            }
        }
    }

    if (bucket->size == bucket->capacity) {
        size_t capacity = bucket->capacity * 2;
        struct htable_entry *entries = realloc(bucket->entries, capacity * sizeof(struct htable_entry));
        if (unlikely(entries == NULL)) {
            rwlock_write_unlock(&bucket->lock);
            return NULL;
        }

        bucket->entries = entries;
        bucket->capacity = capacity;
    }

    struct htable_entry *entry = &bucket->entries[bucket->size];
    entry->hash = hash;
    entry->key = key;
    entry->key_size = key_size;
    entry->value = value;
    bucket->size++;
    atomic_fetch_add_explicit(&ht->size, 1, memory_order_relaxed);
    rwlock_write_unlock(&bucket->lock);

    return value;
}


#ifdef __cplusplus
}
#endif
#endif
