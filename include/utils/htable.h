#ifndef BFLD_UTILS_CONCURRENT_HASH_TABLE_H
#define BFLD_UTILS_CONCURRENT_HASH_TABLE_H
#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdatomic.h>
//#include "hash.h"
#include "cdefs.h"


#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#include <threads.h>
#elif defined(_POSIX_C_SOURCE) && _POSIX_C_SOURCE >= 199309L
#include <sched.h>
#define thrd_yield() sched_yield()
#endif


struct htable_slot
{
    _Atomic uint64_t hash;
    size_t key_size;
    const void *key;
    void * _Atomic value;
};


struct htable
{
    struct htable_slot *slots;
    atomic_size_t size;
    size_t capacity;
};


/*
 * Don't allow insertions if we're more than 75% full.
 */
#define HTABLE_LIMIT(ht_ptr) \
    (((ht_ptr)->capacity >> 2) * 3)


void htable_init(struct htable *ht, size_t capacity);


void htable_free(struct htable *ht);


static inline
size_t htable_size(const struct htable *ht)
{
    return atomic_load_explicit(&ht->size, memory_order_relaxed);
}


static inline
size_t htable_capacity(const struct htable *ht)
{
    return ht->capacity;
}


static inline
void * htable_get(const struct htable *ht,
                  uint64_t hash,
                  const void *key, 
                  size_t key_size)
{
    if (unlikely(hash == 0)) {
        hash = 1; // hash == 0 means unused
    }
    
    const struct htable_slot *slots = ht->slots;

    const struct htable_slot *slot;
    uint64_t slot_hash;
    size_t idx;
   
    for (idx = hash & (ht->capacity - 1), slot = &slots[idx];
            (slot_hash = atomic_load_explicit(&slot->hash, memory_order_acquire)) != 0;
            idx = (idx + 1) & (ht->capacity - 1), slot = &slots[idx]) {

        if (slot_hash == hash) {
            // Wait until slot is ready
            void *value = atomic_load_explicit(&slot->value, memory_order_acquire);
            while (value == NULL) {
#if defined(__x86_64__) || defined(__i386__)
                __asm__ __volatile__("pause");
#elif defined(__aarch64__)
                __asm__ __volatile__("yield");
#else
                thrd_yield();
#endif
                value = atomic_load_explicit(&slot->value, memory_order_acquire);
            }

            if (slot->key_size == key_size
                    && (slot->key == key || !memcmp(slot->key, key, key_size))) {
                return value;
            }
        }
    }

    return NULL;
}



static inline
void * htable_put(struct htable *ht,
                  uint64_t hash,
                  const void *key,
                  size_t key_size,
                  void *value)
{
    if (unlikely(value == NULL)) {
        return NULL;
    }

    size_t size = atomic_fetch_add_explicit(&ht->size, 1, memory_order_release);
    if (unlikely(size >= HTABLE_LIMIT(ht))) {
        atomic_fetch_sub_explicit(&ht->size, 1, memory_order_relaxed);
        return htable_get(ht, hash, key, key_size);
    }

    if (unlikely(hash == 0)) {
        hash = 1;
    }

    size_t idx = hash & (ht->capacity - 1);
    for (;;) {
        struct htable_slot *slot = &ht->slots[idx];
        uint64_t slot_hash = atomic_load_explicit(&slot->hash, memory_order_acquire);

        // If the slot is empty attempt to take it
        if (slot_hash == 0) {
            if (atomic_compare_exchange_strong_explicit(&slot->hash, &slot_hash, hash,
                                                        memory_order_release,
                                                        memory_order_acquire)) {
                // We got the slot
                slot->key_size = key_size;
                slot->key = key;
                atomic_store_explicit(&slot->value, value, memory_order_release);
                return value;
            }
        }

        // Slot was taken, check if it is the same key
        if (slot_hash == hash) {
            void *slot_value = atomic_load_explicit(&slot->value, memory_order_acquire);
            while (slot_value == NULL) {
#if defined(__x86_64__) || defined(__i386__)
                __asm__ __volatile__("pause");
#elif defined(__aarch64__)
                __asm__ __volatile__("yield");
#else
                thrd_yield();
#endif
                slot_value = atomic_load_explicit(&slot->value, memory_order_acquire);
            }

            if (slot->key_size == key_size && 
                    (slot->key == key || !memcmp(slot->key, key, key_size))) {
                atomic_fetch_sub_explicit(&ht->size, 1, memory_order_release);
                return slot_value;
            }
        }
        
        idx = (idx + 1) & (ht->capacity - 1);
    }
}


static inline
bool htable_put_check(struct htable *ht, uint64_t hash, 
                      const void *key, size_t key_size, void *value)
{
    void *ptr = htable_put(ht, hash, key, key_size, value);
    return ptr == value;
}


#ifdef __cplusplus
}
#endif
#endif
