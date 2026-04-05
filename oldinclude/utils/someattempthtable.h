#ifndef BFLD_LOCKFREE_HASH_TABLE2_H
#define BFLD_LOCKFREE_HASH_TABLE2_H
#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdatomic.h>
#include <stdalign.h>
#include "cdefs.h"
#include "align.h"


#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#include <threads.h>
#elif defined(_POSIX_C_SOURCE) && _POSIX_C_SOURCE >= 199309L
#include <sched.h>
#define thrd_yield() sched_yield()
#else
#define thrd_yield()
#endif


struct htable_entry
{
    const void *key;
    size_t key_size;
};


struct htable_slot;


/*
 * Lock-free, insert-only hash table implementation.
 *
 * Implementation of a concurrent hash table using open adressing
 * with linear probing. It is insert-only and does not support 
 * resizing, thus avoiding challenges with deletion and rehashing.
 * 
 * The hash table avoids locks by using C11 compare-and-swap atomics,
 * allowing concurrent searches and insertions. However, a major
 * downside is that the entire capacity must be reserved in advance.
 *
 * Note that this implementation assumes that pointers to keys and their 
 * associated values are stable, and pointing to memory that that has
 * a life time at least as long as the hash table.
 */
struct htable
{
    struct htable_slot *slots;  // hash table entries
    atomic_size_t size;         // number of entries used
    size_t capacity;            // the total capacity of the table (is a power of two to avoid modulo operations)
};


/*
 * Hash table slot/entry.
 *
 * A hash of 0 indicates that the slot is free and not taken
 * for an entry. However, a reader must wait until value is
 * non-NULL before comparing the key (NULL values are not allowed).
 */
struct htable_slot
{
    //_Atomic uint64_t hash;      // entry's hash (0 if slot is free)
    _Atomic size_t key_size;     // size of the entry's key
    const void *key;            // pointer to key (must be stable)
    void * _Atomic value;       // pointer to the associated value (must be stable)
};


/*
 * Initialize the hash table with at least the given capacity.
 */
void htable_init(struct htable *ht, size_t capacity);


/*
 * Free the memory allocated by the hash table.
 * Note that this is not thread-safe, and should only
 * be done once it can be guaranteed that threads are 
 * no longer accessing the table.
 */
void htable_free(struct htable *ht);


/*
 * Helper function to get the size of the table (number of entries).
 */
static inline
size_t htable_size(const struct htable *ht)
{
    return atomic_load_explicit(&ht->size, memory_order_relaxed);
}


/*
 * Helper function to get the capacity of the hash table.
 */
static inline
size_t htable_capacity(const struct htable *ht)
{
    return ht->capacity;
}


/*
 * Look up a value in the hash table.
 *
 * Takes in a key and the calculated hash value of the key,
 * and attempts to find an entry in the table.
 *
 * Returns the associated value if an entry with the same
 * key was found in the table, or NULL otherwise.
 */
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
    uint16_t slot_hash;
    size_t idx;
   
    for (idx = hash & (ht->capacity - 1), slot = &slots[idx];
            (slot_hash = atomic_load_explicit(&slot->key_size, memory_order_acquire)) != 0;
            idx = (idx + 1) & (ht->capacity - 1), slot = &slots[idx]) {

        if (likely(slot_hash == key_size)) {
            // Wait until slot is ready
            void *value = atomic_load_explicit(&slot->value, memory_order_acquire);
            while (unlikely(value == NULL)) {
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


/*
 * Insert a value in the hash table with an associated key.
 *
 * Tries to find an empty slot for the given key and calculated hash,
 * in the hash table. If the key already exists in the table, the
 * insertion is stopped and the already associated value is returned.
 * If an empty slot is found, the value and key are inserted and
 * the same pointer as value is returned. If something went wrong, 
 * NULL is returned.
 *
 * In effect, this is the same as get_or_put() as the value is
 * inserted if the key is not already found in the table.
 *
 * Note that the pointers to key and value must be stable and
 * point to memory with a lifetime that is guaranteed to be as
 * long as the life time of the hash table.
 *
 * Note also that value can not be NULL.
 */
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

    if (unlikely(ht->capacity == 0)) {
        return NULL;
    }

    size_t size = atomic_fetch_add_explicit(&ht->size, 1, memory_order_release);
    if (unlikely(size >= ht->capacity - 1)) {
        // Table is full and trying to insert would lead to looping forever
        atomic_fetch_sub_explicit(&ht->size, 1, memory_order_relaxed);
        return htable_get(ht, hash, key, key_size);
    }

    if (unlikely(hash == 0)) {
        hash = 1; // hash == 0 means unused slot
    }

    size_t idx = hash & (ht->capacity - 1);
    for (;;) {
        struct htable_slot *slot = &ht->slots[idx];
        uint16_t slot_hash = atomic_load_explicit(&slot->key_size, memory_order_acquire);

        // If the slot is empty attempt to take it
        if (likely(slot_hash == 0)) {
            if (atomic_compare_exchange_strong_explicit(&slot->key_size, &slot_hash, key_size,
                                                        memory_order_release,
                                                        memory_order_acquire)) {
                // We got the slot
                //slot->key_size = key_size;
                slot->key = key;
                atomic_store_explicit(&slot->value, value, memory_order_release);
                return value;
            }
        }

        // Slot was taken, check if it is the same key
        if (likely(slot_hash == key_size)) {
            void *slot_value = atomic_load_explicit(&slot->value, memory_order_acquire);
            while (unlikely(slot_value == NULL)) {
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


/*
 * A variant of htable_put() that returns true if the key
 * didn't exist already in the table and the new value is inserted,
 * and false if the key was already found.
 */
static inline
bool htable_put_check(struct htable *ht, uint64_t hash, 
                      const void *key, size_t key_size, void *value)
{
    if (unlikely(value == NULL)) {
        return false;
    }

    void *ptr = htable_put(ht, hash, key, key_size, value);
    return ptr == value;
}


#ifdef __cplusplus
}
#endif
#endif
