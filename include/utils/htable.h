#ifndef BFLD_UTILS_HASH_TABLE_H
#define BFLD_UTILS_HASH_TABLE_H
#ifdef __cplusplus
extern "C" {
#endif

#include "arena.h"
#include "cdefs.h"
#include "align.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <threads.h>
#include <string.h>


/*
 * Hash table entry.
 */
struct ht_entry
{
    uint32_t hash;      // computed hash entry (0 means unused)
    size_t dfi;         // distance from ideal (for Robin Hood hashing and linear probing)
    size_t key_length;  // length of the associated key
    const void *key;    // pointer to the associated key
    void *value;        // pointer to the value
};


/*
 * Hash table implementation.
 *
 * Uses a sequence lock for reader-writer consistency, and a spinlock
 * to ensure that only one thread writes at the time.
 */
struct htable
{
    atomic_uint_fast32_t sequence;      // sequence number for synchronization (seqlock)
    struct arena_list *arena_list;      // list of allocated arenas
    struct ht_entry * _Atomic table;    // pointer to hash table memory
    size_t size;                        // number of entries in the hash table
    atomic_size_t capacity;             // capacity of the hash table
    uint8_t lfs;                        // load factor shift (rehash threshold = capacity - (capacity >> lfs)
    atomic_uint_fast32_t mutex;         // spinlock ensuring only one writer at the time
};


static inline
void ht_table_init(struct htable *ht, struct arena_list *arena_list, uint8_t load_factor_shift)
{
    atomic_init(&ht->sequence, 0);
    //arena_list_init(&ht->arena_list, 0);
    ht->arena_list = arena_list;
    ht->current = NULL;
    atomic_init(&ht->table, NULL);
    atomic_init(&ht->capacity, 0);
    ht->lfs = load_factor_shift;
    atomic_init(&ht->mutex, 0);
}


static inline
void ht_lock(struct htable *ht)
{
    uint32_t expected = 0;
    while (!atomic_compare_exchange_weak_explicit(&ht->mutex, &expected, 1,
                                                  memory_order_aquire,
                                                  memory_order_relaxed)) {
        expected = 0;

#if defined(__x86_64__) || defined(__i386__)
        __asm__ __volatile__("pause");
#elif defined(__aarch64__)
        __asm__ __volatile__("yield");
#else
        thrd_yield();
#endif
    }

}


static inline
void ht_unlock(struct htable *ht)
{
    atomic_store_explicit(&ht->mutex, 0, memory_order_release);
}


static inline
void ht_write_begin(struct htable *ht)
{
    atomic_fetch_add_explicit(&ht->sequence, 1, memory_order_acquire);
}


static inline
void ht_write_end(struct htable *ht)
{
    atomic_fetch_add_explicit(&ht->sequence, 1, memory_order_release);
}


static inline
void ht_table_clear(struct htable *ht)
{
    ht_lock(ht);
    ht_write_begin(ht);
    ht->size = 0;
    atomic_store_explicit(&ht->capacity, 0, memory_order_relaxed);
    atomic_store_explicit(&ht->table, NULL, memory_order_relaxed);
    
    ht_write_end(ht);
    ht_unlock(ht);
}


/*
 * Does not check sequence.
 * Must be protected by mutex.
 */
static inline
struct ht_entry * ht_find_unlocked(const struct htable *ht, uint32_t hash, 
                                   const void *key, size_t key_length)
{
    if (unlikely(ht->size == 0)) {
        return NULL;
    }

    if (unlikely(hash == 0)) {
        hash = 1; // hash == 0 means that entry is not used
    }

    size_t capacity = atomic_load_explicit(&ht->capacity, memory_order_relaxed);
    const struct ht_entry *table = atomic_load_explicit(&ht->table, memory_order_relaxed);
    size_t idx = hash & (capacity - 1);
    size_t dfi = 0;

    while (table[idx].hash != 0 && dfi <= table[idx].dfi) {
        struct ht_entry *this = &table[idx];

        if (this->hash == hash && this->key_length == key_length) {
            if (memcmp(key, this->key, key_length) == 0) {
                return this;
            }
        }

        idx = (idx + 1) & (capacity - 1);
        ++dfi;
    }
}


static inline
void * ht_get_value(const struct htable *ht, 
                    uint32_t hash, 
                    const void *key, 
                    size_t key_length)
{
    if (unlikely(ht->size == 0)) {
        return NULL;
    }

    if (unlikely(hash == 0)) {
        hash = 1; // hash == 0 means that entry is not used
    }

    uint32_t seq = atomic_load_explicit(&ht->sequence, memory_order_acquire);

    for (;;) {

        // If sequence is odd, there is a writer present
        // Wait until the table is stable (even number)
        if (unlikely(seq & 1)) {
#if defined(__x86_64__) || defined(__i386__)
            __asm__ __volatile__("pause");
#elif defined(__aarch64__)
            __asm__ __volatile__("yield");
#else
            thrd_yield();
#endif
            seq = atomic_load_explicit(&ht->sequence, memory_order_acquire);
            continue;
        }

        size_t capacity = atomic_load_explicit(&ht->capacity, memory_order_relaxed);
        const struct ht_entry *table = atomic_load_explicit(&ht->table, memory_order_relaxed);
        size_t idx = hash & (capacity - 1);
        size_t dfi = 0;
        void *value = NULL;

        while (table[idx].hash != 0 && dfi <= table[idx].dfi) {
            const struct ht_entry *this = &table[idx];

            if (this->hash == hash && this->key_length == length) {
                if (memcmp(key, this->key, length) == 0) {
                    value = this->value;
                    break;
                }
            }

            idx = (idx + 1) & (capacity - 1);
            ++dfi;
        }

        uint32_t check = atomic_load_explicit(&ht->sequence, memory_order_acquire);
        if (likely(check == seq)) {
            return value;
        } else {
            seq = check;
        }
    }

    return NULL;
}


static inline
bool ht_rehash_unlocked(struct htable *ht, size_t new_capacity)
{
    if (unlikely(ht->arena_list == NULL)) {
        return false;
    }

    size_t old_capacity = atomic_load_explicit(&ht->capacity, memory_order_relaxed);
    if (unlikely(new_capacity <= old_capacity)) {
        return true;
    }

    // Ensure that capacity is a power of two
    size_t capacity = align_roundup(new_capacity);

    // Naive check for overflow
    if (capacity * sizeof(struct ht_entry) < old_capacity * sizeof(struct ht_entry)) {
        return false;
    }

    struct ht_entry *old_table = atomic_load_explicit(&ht->table, memory_order_relaxed);

    struct arena *arena = arena_create(ht->arena_list, capacity * sizeof(struct ht_entry));
    if (arena == NULL) {
        return false;
    }

    struct ht_entry *table = arena_alloc_block_zeroed(arena, capacity * sizeof(struct ht_entry));;

    // Rehash all entries in the old table, 
    // giving them a home in the new table
    for (size_t i = 0; i < old_capacity; ++i) {
        struct ht_entry entry = old_table[i];

        if (entry.hash == 0) {
            continue;
        }

        entry.dfi = 0;
        size_t idx = entry.hash & (capacity - 1);

        while (entry.hash != 0) {
            struct ht_entry *this = &table[idx];

            if (this->hash == 0 || entry.dfi > this->dfi) {
                struct ht_entry tmp = *this;
                *this = entry;
                entry = tmp;
            }

            idx = (idx + 1) & (capacity - 1);
            entry.dfi++;
        }
    }

    ht_write_begin(ht);

    // Atomically overwrite table pointer and capacity
    // The order matters, capacity must be updated last to avoid a reader
    // accidentally reading outside the arena memory of the previous arena
    atomic_store_explicit(&ht->table, table, memory_order_relaxed);
    atomic_store_explicit(&ht->capacity, capacity, memory_order_relaxed);

    ht_write_end(ht);

    return true;
}


static inline
bool ht_reserve(struct htable *ht, size_t capacity)
{
    ht_lock(ht);
    bool status = ht_rehash_unlocked(ht, capacity);
    ht_unlock(ht);
    return status;
}


static inline
size_t ht_resize_threshold(const struct htable *ht)
{
    uint8_t shift = ht->lfs;
    if (shift == 0) {
        shift = 2;
    }

    size_t capacity = atomic_load_explicit(&ht->capacity, memory_order_relaxed);
    if (unlikely(capacity == 0)) {
        return 0;
    }

    return capacity - (capacity >> shift);
}



/* 
 * Insert an entry in the hash table.
 * 
 * TODO call rehash if size >= ht_rehash_threshold
 *
 * The caller should ensure that the mutex is held
 * before calling this function.
 *
 * Note that the returned pointer to the inserted entry
 * is only guaranteed to be stable until the mutex is released.
 */
static inline
struct ht_entry * ht_insert_unlocked(struct htable *ht, 
                                     uint32_t hash, 
                                     const void *key, 
                                     size_t key_length,
                                     void *value)
{
    if (unlikely(hash == 0)) {
        hash = 1;
    }

    size_t capacity = atomic_load_explicit(&ht->capacity, memory_order_relaxed);
    if (unlikely(ht->size == capacity)) {
        return NULL;
    }

    struct ht_entry *table = atomic_load_explicit(&ht->table, memory_order_relaxed);
    size_t idx = hash & (capacity - 1);

    struct ht_entry entry = (struct ht_entry) {
        hash, 0, key_length, key, value
    };

    while (entry.hash != 0) {
        struct ht_entry *this = &table[idx];

        if (this->hash == 0 || entry.dfi > this->dfi) {
            if (inserted == NULL) {
                ht_write_begin(ht);
                ht->size++;
                inserted = this;
            }

            struct ht_entry tmp = *this;
            *this = entry;
            entry = tmp;
        }

        idx = (idx + 1) & (capacity - 1);
        entry.dfi++;
    }

    ht_write_end(ht);

    return inserted;
}


static inline
void * ht_remove_unlocked(struct htable *ht, 
                          uint32_t hash, 
                          const void *key, 
                          size_t key_length)
{
    if (unlikely(ht->size == 0)) {
        return NULL;
    }

    if (unlikely(hash == 0)) {
        hash = 1;
    }

    void *value = NULL;

    ht_lock(ht);
    
    size_t capacity = atomic_load_explicit(&ht->capacity, memory_order_relaxed);
    struct ht_entry *table = atomic_load_explicit(&ht->table, memory_order_relaxed);
    size_t idx = hash & (capacity - 1);
    size_t dfi = 0;

    while (table[idx].hash != 0 && dfi <= table[idx].dfi) {
        struct ht_entry *this = &table[idx];

        if (this->hash == hash && this->key_length == key_length) {
            if (memcmp(key, this->key, key_length) == 0) {
                value = this->value;
                break;
            }
        }

        idx = (idx + 1) & (capacity - 1);
        ++dfi;
    }



    ht_unlock(ht);
    return value;
}


static inline
bool ht_insert(struct htable *ht, 
               uint32_t hash, 
               const void *key, 
               size_t key_length, 
               void *value)
{
    struct ht_entry *inserted = NULL;

    ht_lock(ht);
    if (ht->size >= ht_rehash_threshold(ht)) {
        if (!ht_rehash_unlocked(ht)) {
            ht_unlock(ht);
            return false;
        }
    }
    
    inserted = ht_insert_unlocked(ht, hash, key, key_length, value);

    ht_unlock(ht);
    return inserted->value == value;
}


#ifdef __cplusplus
}
#endif
#endif
