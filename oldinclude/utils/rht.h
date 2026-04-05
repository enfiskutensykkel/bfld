#ifndef BFLD_UTILS_ROBIN_HOOD_HASH_TABLE_H
#define BFLD_UTILS_ROBIN_HOOD_HASH_TABLE_H
#ifdef __cplusplus
extern "C" {
#endif

#include "cdefs.h"
#include "align.h"
#include "spinlock.h"
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <threads.h>
#include <string.h>


// Forward declaration
struct rht_entry;


/*
 * Robin Hood hash table implementation.
 *
 * Robin Hood hashing uses a distance from ideal counter (DFI) to
 * do open addressing with linear probing on collisions (also sometimes
 * called probe sequence length). The term Robin Hood refers to the fact 
 * that it "steals" from the rich to give to the poor, by shifting elements
 * based on their DFI in order to reduce the maximum DFI any one element has.
 *
 * This implementation supports single writer, multiple readers concurrency; 
 * it uses a sequence number for reader-writer consistency and a spinlock to 
 * ensure that only one thread can write at the time (writer lock). This means 
 * that the ideal use case for this is when there is one writer thread, 
 * as otherwise there would be contention for insertion/deletion/rehashing.
 */
struct rht
{
    atomic_uint_fast32_t sequence;      // sequence number for synchronization (seqlock)
    _Atomic (struct rht_entry*) array;  // pointer to the underlying array memory
    size_t size;                        // number of entries in the hash table
    atomic_size_t capacity;             // capacity of the hash table
    struct spinlock lock;               // spinlock ensuring only one writer at the time (writer lock)
    uint8_t load_factor;                // desired load factor (in percent)
    size_t limit;                       // threshold for when we need to resize the hash table
};


/*
 * Hash table entry.
 */
struct rht_entry
{
    uint64_t hash;      // computed hash entry (0 means unused)
    size_t dfi;         // distance from ideal (for Robin Hood hashing and linear probing)
    size_t key_size;    // length of the associated key
    const void *key;    // pointer to the associated key
    void *value;        // pointer to the value
};


/*
 * Helper macro to ensure that a load factor percent value
 * is within 5%-95% bounds, or uses the default value 75%
 * if pct is 0.
 */
#define RHT_LOAD_FACTOR_BOUNDS(pct) \
    ((pct) == 0 ? 75 : ((pct) < 5) ? 5 : ((pct) > 95) ? 95 : (pct))


/*
 * Helper macro to calculate the necessary capacity (in number of entries)
 * needed for the hash table array given a desired load factor percent.
 */
#define RHT_CAPACITY(entries, load_factor_pct) \
    ((entries) > (SIZE_MAX / 100) ? 0 : \
     align_roundup(((entries) * 100) / (RHT_LOAD_FACTOR_BOUNDS(load_factor_pct))))


#define RHT_BYTES_NEEDED(entries, load_factor_pct) \
    (RHT_CAPACITY((entries), (load_factor_pct)) * sizeof(struct rht_entry))


/*
 * Helper function to get the current size of the hash table (number of entries)
 */
static inline
size_t rht_size(const struct rht *ht)
{
    return ht->size;
}


/*
 * Helper function to get the current capacity of the hash table.
 */
static inline
size_t rht_capacity(const struct rht *ht)
{
    return atomic_load_explicit(&ht->capacity, memory_order_relaxed);
}


/*
 * Helper function to take the hash table writer lock.
 */
static inline
void rht_write_lock_acquire(struct rht *ht)
{
    spinlock_lock(&ht->lock);
}


/*
 * Helper function to release the hash table writer lock
 */
static inline
void rht_write_lock_release(struct rht *ht)
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
void rht_write_begin(struct rht *ht)
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
void rht_write_end(struct rht *ht)
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
size_t rht_find_unlocked(const struct rht *ht, uint64_t hash, 
                        const void *key, size_t key_size)
{
    if (unlikely(hash == 0)) {
        hash = 1; // hash == 0 means unused entry
    }

    size_t capacity = atomic_load_explicit(&ht->capacity, memory_order_relaxed);
    const struct rht_entry *table = atomic_load_explicit(&ht->array, memory_order_relaxed);

    if (unlikely(capacity == 0)) {
        return SIZE_MAX;
    }

    size_t idx = hash & (capacity - 1);
    size_t dfi = 0;

    while (table[idx].hash != 0 && dfi <= table[idx].dfi) {
        const struct rht_entry *this = &table[idx];

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
void * rht_get(const struct rht *ht, uint64_t hash, 
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
    struct rht_entry *table = atomic_load_explicit(&ht->array, memory_order_relaxed);

    size_t idx = hash & (capacity - 1);
    size_t dfi = 0;
    struct rht_entry curr = *((const volatile struct rht_entry*) &table[idx]);

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
        curr = *((const volatile struct rht_entry*) &table[idx]);
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
 * hold at least new_capacity numbers of entries (struct rht_entry).
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
void * rht_rehash_unlocked(struct rht *ht, struct rht_entry *new_table, size_t new_capacity)
{
    size_t capacity = 0;

    size_t old_capacity = atomic_load_explicit(&ht->capacity, memory_order_relaxed);

    if (likely(new_capacity > 0)) {
        // Ensure that capacity is a power of two and that we don't overflow
        capacity = ((size_t) 1ULL) << align_floorlog2(new_capacity);

        if (unlikely(capacity * sizeof(struct rht_entry) < old_capacity * sizeof(struct rht_entry))) {
            return NULL;
        }
    }

    // The new array is not large enough to hold all entries in the current table
    if (unlikely(capacity > 0 && capacity <= ht->size)) {
        return NULL;
    }

    struct rht_entry *old_table = atomic_load_explicit(&ht->array, memory_order_relaxed);

    // Rehash all entries in the old table, 
    // giving them a home in the new array
    struct rht_entry *table = new_table;
    memset(table, 0, capacity * sizeof(struct rht_entry));
    for (size_t i = 0; i < old_capacity; ++i) {
        struct rht_entry entry = old_table[i];

        if (entry.hash == 0) {
            continue;
        }

        entry.dfi = 0;
        size_t idx = entry.hash & (capacity - 1);

        while (entry.hash != 0) {
            struct rht_entry *this = &table[idx];

            if (this->hash == 0 || entry.dfi > this->dfi) {
                struct rht_entry tmp = *this;
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
    rht_write_begin(ht);
    ht->limit = limit;
    atomic_store_explicit(&ht->array, table, memory_order_relaxed);
    atomic_store_explicit(&ht->capacity, capacity, memory_order_relaxed);
    rht_write_end(ht);

    return old_table;
}


#define RHT_INIT (struct rht) {0, NULL, 0, 0, SPINLOCK_INIT, 75, 0}


/*
 * Initialize the hash table.
 *
 * Initializes the members of the hash table and sets the pointer to 
 * the underlying array and the initial capacity. Also sets the desired
 * load factor.
 */
static inline
void rht_init(struct rht *ht, 
              struct rht_entry *array, 
              size_t capacity, 
              uint8_t load_factor_pct)
{
    load_factor_pct = RHT_LOAD_FACTOR_BOUNDS(load_factor_pct);

    if (likely(capacity > 0)) {
        capacity = ((size_t) 1ULL) << align_floorlog2(capacity);
        if (capacity > SIZE_MAX / sizeof(struct rht_entry)) {
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
    spinlock_init(&ht->lock);
    ht->load_factor = load_factor_pct;
    ht->limit = 0;

    rht_rehash_unlocked(ht, array, capacity);
}


static inline
void * rht_rehash(struct rht *ht, struct rht_entry *array, size_t capacity)
{
    rht_write_lock_acquire(ht);
    void *old = rht_rehash_unlocked(ht, array, capacity);
    rht_write_lock_release(ht);
    return old;
}


/*
 * Helper function to clear all entries from the hash table.
 * The caller is responsible for taking the writer lock before
 * calling this.
 */
static inline
void rht_clear_unlocked(struct rht *ht)
{
    if (likely(ht->size > 0)) {
        rht_write_begin(ht);

        size_t capacity = atomic_load_explicit(&ht->capacity, memory_order_relaxed);
        struct rht_entry *table = atomic_load_explicit(&ht->array, memory_order_relaxed);
        memset(table, 0, capacity * sizeof(struct rht_entry));

        rht_write_end(ht);
    }
}


/*
 * Remove all entries in the hash table.
 */
static inline
void rht_clear(struct rht *ht)
{
    rht_write_lock_acquire(ht);
    rht_clear_unlocked(ht);
    rht_write_lock_release(ht);
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
size_t rht_insert_unlocked(struct rht *ht, 
                           uint64_t hash, 
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

    struct rht_entry *table = atomic_load_explicit(&ht->array, memory_order_relaxed);
    size_t idx = hash & (capacity - 1);

    struct rht_entry entry = (struct rht_entry) {
        hash, 0, key_size, key, value
    };

    size_t pos = capacity;
    while (entry.hash != 0) {
        struct rht_entry *this = &table[idx];

        if (this->hash == 0 || entry.dfi > this->dfi) {
            if (pos == capacity) {
                rht_write_begin(ht);
                ht->size++;
                pos = idx;
            }

            struct rht_entry tmp = *this;
            *this = entry;
            entry = tmp;
        }

        idx = (idx + 1) & (capacity - 1);
        entry.dfi++;
    }

    rht_write_end(ht);

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
void rht_remove_unlocked(struct rht *ht, size_t idx)
{
    size_t capacity = atomic_load_explicit(&ht->capacity, memory_order_relaxed);
    struct rht_entry *table = atomic_load_explicit(&ht->array, memory_order_relaxed);
    struct rht_entry *curr = &table[idx];
    struct rht_entry *next = &table[(idx + 1) & (capacity - 1)];

    rht_write_begin(ht);
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

    rht_write_end(ht);
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
void * rht_remove(struct rht *ht, uint64_t hash, 
                 const void *key, size_t key_size)
{
    void *value = NULL;

    rht_write_lock_acquire(ht);
    size_t idx = rht_find_unlocked(ht, hash, key, key_size);
    if (unlikely(idx == SIZE_MAX)) {
        rht_write_lock_release(ht);
        return NULL;
    }

    struct rht_entry *table = atomic_load_explicit(&ht->array, memory_order_relaxed);
    value = table[idx].value;

    rht_remove_unlocked(ht, idx);

    rht_write_lock_release(ht);
    return value;
}


static inline
bool rht_needs_rehash(const struct rht *ht)
{
    return ht->size >= ht->limit;
}


/*
 * Insert an entry in the hash table with the specified key.
 *
 * This function will try to look up the specified key. If an entry
 * with an associated key does not exist, the value is inserted as 
 * a new entry. 
 *
 * The caller must also ensure that the the underlying array
 * is large enough to hold a new, inserted value, by checking 
 * that ht->size + 1 < ht->limit.
 *
 * Note that the pointer to key and value must be stable, and
 * have a lifetime of at least the same as the hash table.
 *
 * Returns 0 if an entry with the associated key was inserted,
 * or a negative value on failure:
 * - Returns -ENOSPC if the underlying table array needs to grow.
 * - Returns -EEXIST if the key was already inserted into the array.
 */
static inline
int rht_insert(struct rht *ht, uint64_t hash, 
               const void *key, size_t key_size,
               void *value)
{
    rht_write_lock_acquire(ht);

    // Do we need to grow the hash table?
    if (ht->size >= ht->limit) {
        rht_write_lock_release(ht);
        return -ENOSPC;
    }

    size_t inserted = rht_find_unlocked(ht, hash, key, key_size);
    
    // Check if entry was already inserted and simply update value if it was
    if (likely(inserted != SIZE_MAX)) {
        rht_write_lock_release(ht);
        return -EEXIST;
    }

    // Do the actual insertion
    rht_insert_unlocked(ht, hash, key, key_size, value);

    rht_write_lock_release(ht);
    return 0;
}


#ifdef __cplusplus
}
#endif
#endif
