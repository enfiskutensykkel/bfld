#ifndef BFLD_UTILS_HASH_TABLE_H
#define BFLD_UTILS_HASH_TABLE_H
#ifdef __cplusplus
extern "C" {
#endif

#include "cdefs.h"
#include "align.h"
#include "spinlock.h"
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
    size_t key_size;    // length of the associated key
    const void *key;    // pointer to the associated key
    void *value;        // pointer to the value
};


/*
 * Concurrent hash table implementation.
 *
 * This implementation uses a distance from ideal counter (DFI)
 * and a Robin Hood mechanism to do linear probing on collisions.
 * 
 * Uses a sequence number for reader-writer consistency, and a spinlock
 * to ensure that only one thread writes at the time (writer lock).
 */
struct htable
{
    atomic_uint_fast32_t sequence;      // sequence number for synchronization (seqlock)
    _Atomic (struct ht_entry*) array;   // pointer to the underlying array memory
    size_t size;                        // number of entries in the hash table
    atomic_size_t capacity;             // capacity of the hash table
    struct spinlock lock;               // spinlock ensuring only one writer at the time (writer lock)
    uint8_t load_factor;                // desired load factor (in percent)
    size_t limit;                       // threshold for when we need to resize the hash table
};


/*
 * Helper function to get the current size of the hash table (number of entries)
 */
static inline
size_t ht_size(const struct htable *ht)
{
    return ht->size;
}


/*
 * Helper function to get the current capacity of the hash table.
 */
static inline
size_t ht_capacity(const struct htable *ht)
{
    return atomic_load_explicit(&ht->capacity, memory_order_relaxed);
}



/*
 * Helper function to take the hash table writer lock.
 */
static inline
void ht_write_lock_acquire(struct htable *ht)
{
    spinlock_lock(&ht->lock);
}


/*
 * Helper function to release the hash table writer lock
 */
static inline
void ht_write_lock_release(struct htable *ht)
{
    spinlock_unlock(&ht->lock);
}


/*
 * Helper function to notify readers that a change is in progress and 
 * that the hash table is unstable.
 *
 * Sets the sequence counter to an odd number.
 *
 * Note that this function does not take the writer lock.
 */
static inline
void ht_write_begin(struct htable *ht)
{
    atomic_fetch_add_explicit(&ht->sequence, 1, memory_order_release);
}


/*
 * Helper function to notify readers that a change has been completed,
 * and that the hash table is stable.
 *
 * Sets the sequence counter to an even number.
 *
 * Note that this function does not release the writer lock.
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
 * The caller is responsible for taking the writer lock
 * before calling this function. Note that the returned index 
 * is only guaranteed to be stable until the writer lock is released.
 */
static inline
size_t ht_find_unlocked(const struct htable *ht, uint32_t hash, 
                        const void *key, size_t key_size)
{
    if (unlikely(hash == 0)) {
        hash = 1; // hash == 0 means unused entry
    }

    size_t capacity = atomic_load_explicit(&ht->capacity, memory_order_relaxed);
    const struct ht_entry *table = atomic_load_explicit(&ht->array, memory_order_relaxed);

    if (unlikely(capacity == 0)) {
        return SIZE_MAX;
    }

    size_t idx = hash & (capacity - 1);
    size_t dfi = 0;

    while (table[idx].hash != 0 && dfi <= table[idx].dfi) {
        const struct ht_entry *this = &table[idx];

        if (this->hash == hash && this->key_size == key_size) {
            if (memcmp(this->key, key, key_size) == 0) {
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
              const void *key, size_t key_size)
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
    struct ht_entry *table = atomic_load_explicit(&ht->array, memory_order_relaxed);

    size_t idx = hash & (capacity - 1);
    size_t dfi = 0;
    struct ht_entry curr = *((const volatile struct ht_entry*) &table[idx]);

    while (curr.hash != 0 && dfi <= curr.dfi) {
        if (curr.hash == hash && curr.key_size == key_size) {
            next = atomic_load_explicit(&ht->sequence, memory_order_acquire);
            if (unlikely(next != seq)) {
                seq = next;
                goto retry;
            }

            if (memcmp(curr.key, key, key_size) == 0) {
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
 * Helper function to replace the underlying array memory,
 * while holding the writer lock.
 *
 * Copies and rehashes entries in the existing hash table to the
 * new array. The new_capacity argument must be a power of two, 
 * and the array pointed to by new_table must be large enough to 
 * hold at least new_capacity numbers of entries (struct ht_entry).
 * 
 * On success this function returns a pointer to the old table, 
 * which must be freed by the caller. Note that there 
 *
 * On failure, this function returns NULL.
 *
 * The caller is responsible for taking the write lock before
 * calling this function.
 */
static inline
void * ht_rehash_unlocked(struct htable *ht, struct ht_entry *new_table, size_t new_capacity)
{
    size_t capacity = 0;

    if (likely(new_capacity > 0)) {
        // Ensure that capacity is a power of two and that we don't overflow
        capacity = ((size_t) 1ULL) << align_floorlog2(new_capacity);

        if (unlikely(capacity * sizeof(struct ht_entry) < old_capacity * sizeof(struct ht_entry))) {
            return NULL;
        }
    }

    // The new array is not large enough to hold all entries in the current table
    if (unlikely(capacity > 0 && capacity <= ht->size)) {
        return NULL;
    }

    size_t old_capacity = atomic_load_explicit(&ht->capacity, memory_order_relaxed);
    struct ht_entry *old_table = atomic_load_explicit(&ht->array, memory_order_relaxed);

    // Rehash all entries in the old table, 
    // giving them a home in the new array
    struct ht_entry *table = new_table;
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

    // Calculate new limit
    size_t limit = capacity - 1;  // if capacity<=32 just use a full table as limit
    if (likely(capacity > 32)) {

        // Do some bounds checking
        if (unlikely(ht->load_factor == 0)) {
            ht->load_factor = 75;
        }

        if (unlikely(ht->load_factor < 5)) {
            ht->load_factor = 5;
        }

        if (unlikely(ht->load_factor > 95)) {
            ht->load_factor = 95;
        }
        
        size_t p = ((size_t) ht->load_factor) * 655;  // 655 / 65536 is approximately 0.01
        
        // Find the shift value for capacity
        // nearest k where 2^k ~= capacity
        uint8_t k = align_floorlog2(capacity);
        if (k >= 16) {
            // capacity is equal to, or greater than, 65536
            limit = p << (k - 16);
        } else {
            limit = p >> (16 - k);
        }
    }

    // Atomically overwrite array pointer and capacity
    // The order matters, capacity must be updated last to avoid a reader
    // accidentally reading outside the memory of the previous array.
    ht_write_begin(ht);
    ht->limit = limit;
    atomic_store_explicit(&ht->array, table, memory_order_relaxed);
    atomic_store_explicit(&ht->capacity, capacity, memory_order_relaxed);
    ht_write_end(ht);

    return old_table;
}


/*
 * Initialize the hash table.
 *
 * Initializes the members of the hash table and sets the pointer to 
 * the underlying array and the initial capacity. Also sets the desired
 * load factor.
 */
static inline
void ht_init(struct htable *ht, 
             struct ht_entry *array, 
             size_t capacity, 
             uint8_t load_factor_percent)
{
    if (load_factor_percent == 0) {
        load_factor_percent = 75;
    }

    if (load_factor_percent < 5) {
        load_factor_percent = 5;
    }

    if (load_factor_percent > 95) {
        load_factor_percent = 95;
    }

    if (likely(capacity > 0)) {
        capacity = ((size_t) 1ULL) << align_floorlog2(capacity);
        if (capacity > SIZE_MAX / sizeof(struct ht_entry)) {
            capacity = 0;
        }
    }

    if (unlikely(capacity == 0)) {
        array = NULL;
    }

    atomic_init(&ht->sequence, 0);
    atomic_init(&ht->array, NULL);
    atomic_store_explicit(&ht->array, NULL, memory_order_relaxed);
    ht->size = 0;
    atomic_init(&ht->capacity, 0);
    atomic_store_explicit(&ht->capacity, 0, memory_order_relaxed);
    atomic_init(&ht->lock, 0);
    atomic_store_explicit(&ht->lock, 0, memory_order_relaxed);
    ht->load_factor = load_factor_percent;
    ht->limit = 0;

    ht_rehash_unlocked(ht, array, capacity);
}


/*
 * Helper function to clear all entries from the hash table.
 * The caller is responsible for taking the writer lock before
 * calling this.
 */
static inline
void ht_clear_unlocked(struct htable *ht)
{
    if (likely(ht->size > 0)) {
        ht_write_begin(ht);

        size_t capacity = atomic_load_explicit(&ht->capacity, memory_order_relaxed);
        struct ht_entry *table = atomic_load_explicit(&ht->array, memory_order_relaxed);
        memset(table, 0, capacity * sizeof(struct ht_entry));

        ht_write_end(ht);
    }
}


/*
 * Remove all entries in the hash table.
 */
static inline
void ht_clear(struct htable *ht)
{
    ht_write_lock_acquire(ht);
    ht_clear_unlocked(ht);
    ht_write_lock_release(ht);
}


/* 
 * Helper function to insert an entry in the hash table while
 * holding the writer lock.
 * 
 * Note that the pointer to key and value must be stable, and
 * have a lifetime of at least the same as the hash table.
 *
 * This function will find a suitable index to insert the new
 * entry at. The caller is responsible for making sure
 * that the key does not exist in the table beforehand.
 *
 * The caller must also ensure that the the underlying array
 * is large enough to hold a new, inserted value, by checking 
 * that ht->size + 1 < ht->limit.
 *
 * The caller should ensure that the writer lock is held
 * before calling this function. The returned index to the 
 * inserted entry is only guaranteed to be stable until 
 * the lock is released.
 *
 * On success, this function returns the index to the inserted
 * entry. On failure, this function returns SIZE_MAX. Note that
 * the index is only guaranteed to be stable as long as the writer
 * lock is held.
 */
static inline
size_t ht_insert_unlocked(struct htable *ht, 
                          uint32_t hash, 
                          const void *key, 
                          size_t key_size,
                          void *value)
{
    if (unlikely(hash == 0)) {
        hash = 1; // hash == 0 means unused entry
    }

    size_t capacity = atomic_load_explicit(&ht->capacity, memory_order_relaxed);
    if (unlikely(ht->size >= capacity - 1)) {
        return SIZE_MAX;
    }

    struct ht_entry *table = atomic_load_explicit(&ht->array, memory_order_relaxed);
    size_t idx = hash & (capacity - 1);

    struct ht_entry entry = (struct ht_entry) {
        hash, 0, key_size, key, value
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
 * Helper function to remove an entry from the hash table while holding
 * the writer lock.
 *
 * Does backwards shift deletion from the specified index, 
 * in order to maintain distance from ideal (DFI).
 */
static inline
void ht_remove_unlocked(struct htable *ht, size_t idx)
{
    size_t capacity = atomic_load_explicit(&ht->capacity, memory_order_relaxed);
    struct ht_entry *table = atomic_load_explicit(&ht->array, memory_order_relaxed);
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

    // Zero out the last element
    curr->hash = 0;
    curr->dfi = 0;
    curr->key = NULL;
    curr->key_size = 0;
    curr->value = NULL;

    ht_write_end(ht);
}


/*
 * Remove an entry from the hash table.
 *
 * This function tries to look up an entry from its key,
 * and remove it from the table if it was found.
 *
 * Returns the associated value if the entry was found,
 * or NULL if there was no entry with the specified key.
 */
static inline
void * ht_remove(struct htable *ht, uint32_t hash, 
                 const void *key, size_t key_size)
{
    void *value = NULL;

    ht_write_lock_acquire(ht);
    size_t idx = ht_find_unlocked(ht, hash, key, key_size);
    if (unlikely(idx == SIZE_MAX)) {
        ht_write_lock_release(ht);
        return NULL;
    }

    struct ht_entry *table = atomic_load_explicit(&ht->array, memory_order_relaxed);
    value = table[idx].value;

    ht_remove_unlocked(ht, idx);

    ht_write_lock_release(ht);
    return value;
}


static inline
bool ht_needs_rehash(const struct htable *ht)
{
    return ht->size >= ht->limit;
}



/*
 * Insert or update an entry in the hash table with the specified key.
 *
 * This function will try to look up the specified key. If an entry
 * with an associated key does not exist, the value is inserted as 
 * a new entry. If it already exists, the value is updated.
 *
 * If old_value is non-NULL and the key already exist in the table,
 * old_value is set to the previous value.
 *
 * The caller must also ensure that the the underlying array
 * is large enough to hold a new, inserted value, by checking 
 * that ht->size + 1 < ht->limit.
 *
 * Note that the pointer to key and value must be stable, and
 * have a lifetime of at least the same as the hash table.
 *
 * Returns true if an entry with the associated key was updated
 * or inserted, or false if the table needs to grow and rehashed.
 */
static inline
bool ht_insert(struct htable *ht, uint32_t hash, 
               const void *key, size_t key_size,
               void *value, void **old_value)
{
    ht_write_lock_acquire(ht);
    size_t inserted = ht_find_unlocked(ht, hash, key, key_size);

    // Check if entry was already inserted and simply update value if it was
    if (likely(inserted != SIZE_MAX)) {
        struct ht_entry *table = atomic_load_explicit(&ht->table, memory_order_relaxed);

        ht_write_begin(ht);
        if (old_value != NULL) {
            *old_value = table[inserted].value;
        }
        table[inserted].value = value;
        ht_write_end(ht);
        ht_write_lock_release(ht);
        return true;
    }

    if (old_value != NULL) {
        *old_value = NULL;
    }
    
    // Do we need to grow the hash table?
    if (ht->size >= ht->limit) {
        return false;
    }

    // Do the actual insertion
    ht_insert_unlocked(ht, hash, key, key_size, value);

    ht_write_lock_release(ht);
    return true;
}

#ifdef __cplusplus
}
#endif
#endif
