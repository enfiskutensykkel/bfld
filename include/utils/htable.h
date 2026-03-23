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
    ht->arena_list = arena_list;
    atomic_init(&ht->table, NULL);
    atomic_init(&ht->capacity, 0);
    ht->lfs = load_factor_shift;
    atomic_init(&ht->mutex, 0);
}


/*
 * Helper function to take the hash table writer mutex.
 */
static inline
void ht_lock(struct htable *ht)
{
    uint_fast32_t expected = 0;
    while (!atomic_compare_exchange_weak_explicit(&ht->mutex, &expected, 1,
                                                  memory_order_acquire,
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


/*
 * Helper function to release the hash table writer mutex.
 */
static inline
void ht_unlock(struct htable *ht)
{
    atomic_store_explicit(&ht->mutex, 0, memory_order_release);
}


/*
 * Helper function to notify readers that a change is in progress.
 *
 * Sets the sequence counter to an odd number.
 */
static inline
void ht_write_begin(struct htable *ht)
{
    atomic_fetch_add_explicit(&ht->sequence, 1, memory_order_acq_rel);
}


/*
 * Helper function to notify readers that a change has been completed,
 * and that the hash table is stable.
 *
 * Sets the sequence counter to an even number.
 */
static inline
void ht_write_end(struct htable *ht)
{
    atomic_fetch_add_explicit(&ht->sequence, 1, memory_order_release);
}


/*
 * Helper function to look up an entry from its key.
 *
 * Returns the index to the entry if it was found, or
 * SIZE_MAX if there is no entry with the specified key.
 *
 * The caller is responsible for taking the writer mutex 
 * before calling this function. Note that the returned index 
 * is only guaranteed to be stable until the mutex is released.
 */
static inline
size_t ht_find_unlocked(const struct htable *ht, uint32_t hash, 
                        const void *key, size_t key_length)
{
    size_t capacity = atomic_load_explicit(&ht->capacity, memory_order_relaxed);
    const struct ht_entry *table = atomic_load_explicit(&ht->table, memory_order_relaxed);

    size_t idx = hash & (capacity - 1);
    size_t dfi = 0;

    while (table[idx].hash != 0 && dfi <= table[idx].dfi) {
        const struct ht_entry *this = &table[idx];

        if (this->hash == hash && this->key_length == key_length) {
            if (memcmp(this->key, key, key_length) == 0) {
                return idx;
            }
        }

        idx = (idx + 1) & (capacity - 1);
        ++dfi;
    }

    return SIZE_MAX;
}


/*
 * Look up a value from a key.
 *
 * Finds an entry in the hash table from a given key
 * and returns the associated value.
 *
 * Returns NULL if the key is not found in the hash table.
 */
static inline
void * ht_get(const struct htable *ht, uint32_t hash, 
              const void *key, size_t key_length)
{
    void *value = NULL;
    uint32_t seq = atomic_load_explicit(&ht->sequence, memory_order_acquire);
    uint32_t next;

    if (unlikely(hash == 0)) {
        hash = 1; // hash == 0 means unused entry
    }

retry:
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
        goto retry;
    }

    size_t capacity = atomic_load_explicit(&ht->capacity, memory_order_relaxed);
    struct ht_entry *table = atomic_load_explicit(&ht->table, memory_order_relaxed);

    size_t idx = hash & (capacity - 1);
    size_t dfi = 0;
    struct ht_entry curr = *((const volatile struct ht_entry*) &table[idx]);

    while (curr.hash != 0 && dfi <= curr.dfi) {
        if (curr.hash == hash && curr.key_length == key_length) {
            next = atomic_load_explicit(&ht->sequence, memory_order_acquire);
            if (unlikely(next != seq)) {
                seq = next;
                goto retry;
            }

            if (memcmp(curr.key, key, key_length) == 0) {
                value = curr.value;
                break;
            }
        }

        idx = (idx + 1) & (capacity - 1);
        curr = *((const volatile struct ht_entry*) &table[idx]);
        ++dfi;
    }

    next = atomic_load_explicit(&ht->sequence, memory_order_acquire);
    if (unlikely(next != next)) {
        seq = next;
        goto retry;
    }

    return value;
}


/*
 * Helper function to resize and rehash the hash table to a given capacity.
 * The caller is responsible for taking the writer mutex before calling
 * this function.
 */
static inline
bool ht_resize_unlocked(struct htable *ht, size_t new_capacity)
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

    struct ht_entry *table = arena_alloc_block_zeroed(arena, capacity * sizeof(struct ht_entry), 16);

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

    // Atomically overwrite table pointer and capacity
    // The order matters, capacity must be updated last to avoid a reader
    // accidentally reading outside the arena memory of the previous arena
    ht_write_begin(ht);
    atomic_store_explicit(&ht->table, table, memory_order_relaxed);
    atomic_store_explicit(&ht->capacity, capacity, memory_order_relaxed);
    ht_write_end(ht);

    return true;
}


/*
 * Make sure that the hash table is able to hold at least
 * capacity number of entries.
 */
static inline
bool ht_reserve(struct htable *ht, size_t capacity)
{
    ht_lock(ht);
    bool status = ht_resize_unlocked(ht, capacity);
    ht_unlock(ht);
    return status;
}


/*
 * Remove all entries in the hash table.
 */
static inline
void ht_clear(struct htable *ht)
{
    ht_lock(ht);
    if (likely(ht->size > 0)) {
        ht_write_begin(ht);

        size_t capacity = atomic_load_explicit(&ht->capacity, memory_order_relaxed);
        struct ht_entry *table = atomic_load_explicit(&ht->table, memory_order_relaxed);
        memset(table, 0, capacity * sizeof(struct ht_entry));

        ht_write_end(ht);
    }
    ht_unlock(ht);
}


/*
 * Get the current threshold for when resizing the index is necessary.
 */
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
 * Helper function to insert an entry in the hash table.
 * 
 * This function will find a suitable index to insert the new
 * entry at. The caller is responsible for making sure
 * that the key does not exist in the table beforehand.
 *
 * This implementation uses a distance from ideal counter (DFI)
 * and a Robin Hood mechanism to do linear probing on collisions.
 * 
 * The caller must also ensure that the table is large enough
 * to hold a new, insterted value. Ideally, the table should
 * be resized if ht->size >= ht_rehash_threshold(ht)
 *
 * The caller should ensure that the mutex is held
 * before calling this function. Note that the returned index 
 * to the inserted entry is only guaranteed to be stable until 
 * the mutex is released.
 */
static inline
size_t ht_insert_unlocked(struct htable *ht, 
                          uint32_t hash, 
                          const void *key, 
                          size_t key_length,
                          void *value)
{
    size_t capacity = atomic_load_explicit(&ht->capacity, memory_order_relaxed);
    if (unlikely(ht->size == capacity)) {
        return SIZE_MAX;
    }

    struct ht_entry *table = atomic_load_explicit(&ht->table, memory_order_relaxed);
    size_t idx = hash & (capacity - 1);

    struct ht_entry entry = (struct ht_entry) {
        hash, 0, key_length, key, value
    };

    size_t pos = capacity;
    while (entry.hash != 0) {
        struct ht_entry *this = &table[idx];

        if (this->hash == 0 || entry.dfi > this->dfi) {
            if (pos == capacity) {
                ht_write_begin(ht);
                ht->size++;
                pos = idx;
            }

            struct ht_entry tmp = *this;
            *this = entry;
            entry = tmp;
        }

        idx = (idx + 1) & (capacity - 1);
        entry.dfi++;
    }

    ht_write_end(ht);

    return pos;
}


/*
 * Helper function to remove an entry from the hash table and 
 * do backwards shift deletion, to maintain distance from ideal (DFI).
 */
static inline
void ht_remove_unlocked(struct htable *ht, size_t idx)
{
    size_t capacity = atomic_load_explicit(&ht->capacity, memory_order_relaxed);
    struct ht_entry *table = atomic_load_explicit(&ht->table, memory_order_relaxed);
    struct ht_entry *curr = &table[idx];
    struct ht_entry *next = &table[(idx + 1) & (capacity - 1)];

    ht_write_begin(ht);
    ht->size--;

    // Backwards shift deletion
    while (next->hash != 0 && next->dfi != 0) {
        *curr = *next;
        curr->dfi--;
        curr = next;
        idx = (idx + 1) & (capacity - 1);
        next = &table[idx];
    }

    curr->hash = 0;
    curr->dfi = 0;
    curr->key = NULL;
    curr->key_length = 0;
    curr->value = NULL;

    ht_write_end(ht);
}


/*
 * Remove an entry from the hash table.
 *
 * Look up an entry from its key and delete it.
 *
 * Returns the associated value if the entry was found,
 * or NULL if there was no entry with the specified key.
 */
static inline
void * ht_remove(struct htable *ht, uint32_t hash, 
                 const void *key, size_t key_length)
{
    void *value = NULL;

    if (unlikely(hash == 0)) {
        hash = 1;
    }

    ht_lock(ht);
    size_t idx = ht_find_unlocked(ht, hash, key, key_length);
    if (unlikely(idx == SIZE_MAX)) {
        ht_unlock(ht);
        return NULL;
    }

    struct ht_entry *table = atomic_load_explicit(&ht->table, memory_order_relaxed);
    value = table[idx].value;

    ht_remove_unlocked(ht, idx);

    ht_unlock(ht);
    return value;
}


/*
 * Insert or update an entry in the hash table with the specified key.
 * Returns true if insertion was successful, or false if insertion failed.
 */
static inline
bool ht_insert(struct htable *ht, uint32_t hash, 
               const void *key, size_t key_length, void *value)
{

    if (unlikely(hash == 0)) {
        hash = 1; // hash == 0 means unused entry
    }

    ht_lock(ht);
    size_t inserted = ht_find_unlocked(ht, hash, key, key_length);

    // Check if entry was already inserted and simply update value if it was
    if (likely(inserted != SIZE_MAX)) {
        struct ht_entry *table = atomic_load_explicit(&ht->table, memory_order_relaxed);

        ht_write_begin(ht);
        table[inserted].value = value;
        ht_write_end(ht);
        ht_unlock(ht);
        return true;
    }
    
    // Do we need to resize the hash table?
    if (ht->size >= ht_resize_threshold(ht)) {
        size_t capacity = atomic_load_explicit(&ht->capacity, memory_order_relaxed);
        if (!ht_resize_unlocked(ht, capacity > 0 ? capacity * 2 : 128)) {
            ht_unlock(ht);
            return false;
        }
    }
    
    // Do the actual insertion
    ht_insert_unlocked(ht, hash, key, key_length, value);
    ht_unlock(ht);
    return true;
}


#ifdef __cplusplus
}
#endif
#endif
