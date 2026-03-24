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
    size_t key_size;    // length of the associated key
    const void *key;    // pointer to the associated key
    void *value;        // pointer to the value
};

// FIXME rename size to count
/*
 * Hash table implementation.
 *
 * This implementation uses a distance from ideal counter (DFI)
 * and a Robin Hood mechanism to do linear probing on collisions.
 * 
 * Uses a sequence lock for reader-writer consistency, and a spinlock
 * to ensure that only one thread writes at the time (writer mutex).
 */
struct htable
{
    atomic_uint_fast32_t sequence;      // sequence number for synchronization (seqlock)
    struct arena_list *arena_list;      // list of allocated arenas
    struct arena *arena_cache;          // pointer to current arena used for table memory
    struct ht_entry * _Atomic table;    // pointer to hash table memory
    size_t size;                        // number of entries in the hash table
    atomic_size_t capacity;             // capacity of the hash table
    atomic_uint_fast32_t wrlock;        // spinlock ensuring only one writer at the time (writer lock, mutex)
    uint8_t load_factor;                // desired load factor (in percent)
    size_t limit;                       // threshold for when we need to resize the hash table
};


/*
 * Helper function 
 */
static inline
size_t ht_size(const struct htable *ht)
{
    return ht->size;
}


static inline
size_t ht_capacity(const struct htable *ht)
{
    return atomic_load_explicit(&ht->capacity, memory_order_relaxed);
}


// FIXME: do this directly in resize instead
/*
 * Helper functioin to calculate the limit for a capacity
 * given the desired load factor of the hash table.
 */
static inline
size_t ht_calculate_limit(const struct htable *ht, size_t capacity)
{
    uint8_t p = ht->load_factor;

    // Do some bounds checking
    if (unlikely(p == 0)) {
        p = 75;
    }

    if (unlikely(p < 5)) {
        p = 5;
    }

    if (unlikely(p > 95)) {
        p = 95;
    }

    // 655/65536 ~= 0.01
    size_t pf = ((size_t) p) * 655;

    // Find the shift value for capacity (nearest k where 2^k ~= capacity)
    uint8_t k = align_floorlog2(capacity);

    if (k >= 16) {
        // capacity is equal to or greater than 65536
        return pf << (k - 16);

    } else {

        return pf >> (16 - k);
    }
}


static inline
void ht_init(struct htable *ht, struct arena_list *arena_list, uint8_t load_factor)
{
    if (load_factor == 0) {
        load_factor = 75;
    }

    if (load_factor < 5) {
        load_factor = 5;
    }

    if (load_factor > 95) {
        load_factor = 95;
    }

    atomic_init(&ht->sequence, 0);
    ht->arena_list = arena_list;
    ht->arena_cache = NULL;
    atomic_init(&ht->table, NULL);
    ht->size = 0;
    atomic_init(&ht->capacity, 0);
    atomic_init(&ht->wrlock, 0);
    ht->load_factor = load_factor;
    ht->limit = 0;
}


/*
 * Helper function to take the hash table writer mutex.
 */
static inline
void ht_lock(struct htable *ht)
{
    uint_fast32_t expected = 0;
    while (!atomic_compare_exchange_weak_explicit(&ht->wrlock, &expected, 1,
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
    atomic_store_explicit(&ht->wrlock, 0, memory_order_release);
}


/*
 * Helper function to notify readers that a change is in progress.
 * Sets the sequence counter to an odd number.
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
                        const void *key, size_t key_size)
{
    if (unlikely(hash == 0)) {
        hash = 1; // hash == 0 means unused entry
    }

    size_t capacity = atomic_load_explicit(&ht->capacity, memory_order_relaxed);
    const struct ht_entry *table = atomic_load_explicit(&ht->table, memory_order_relaxed);

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
    struct ht_entry *table = atomic_load_explicit(&ht->table, memory_order_relaxed);

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


// FIXME: this should instead return the old memory so that the caller can pass this to a deferred_free
// "limbo" list


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

    // Ensure that capacity is a power of two and that we don't overflow
    size_t capacity = align_roundup(new_capacity);
    if (capacity * sizeof(struct ht_entry) < old_capacity * sizeof(struct ht_entry)) {
        return false;
    }

    struct ht_entry *old_table = atomic_load_explicit(&ht->table, memory_order_relaxed);

    struct ht_entry *table = arena_alloc_dynamic_zeroed(ht->arena_list, &ht->arena_cache,
                                                        capacity * sizeof(struct ht_entry), 64);
    if (table == NULL) {
        return false;
    }

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
    // accidentally reading outside the aGrena memory of the previous arena
    ht_write_begin(ht);
    if (likely(capacity > 32)) {
        ht->limit = ht_calculate_limit(ht, capacity);
    } else {
        ht->limit = capacity - 1;
    }
    atomic_store_explicit(&ht->table, table, memory_order_relaxed);
    atomic_store_explicit(&ht->capacity, capacity, memory_order_relaxed);
    ht_write_end(ht);

    return true;
}


// FIXME: find a suitable name, perhaps ht_reserve_capacity and ht_extend_capacity?

/*
 * Make sure that the hash table is able to hold at least
 * the specified number of entries.
 */
static inline
bool ht_reserve(struct htable *ht, size_t capacity)
{
    ht_lock(ht);
    
    // Pad the requested capacity with some extra entries
    // to account for the desired load factor
    size_t pad = ht_calculate_limit(ht, capacity);

    if (capacity > SIZE_MAX - pad) {
        ht_unlock(ht);
        return false;
    }

    bool status = ht_resize_unlocked(ht, capacity + pad);
    ht_unlock(ht);
    return status;
}


/*
 * Extend the capacity of the hash table so that it is able
 * to hold 
 */
static inline
bool ht_extend(struct htable *ht, size_t capacity)
{
    ht_lock(ht);
    size_t old_capacity = atomic_load_explicit(&ht->capacity, memory_order_relaxed);

    if (capacity > SIZE_MAX - old_capacity) {
        ht_unlock(ht);
        return false;
    }

    capacity += old_capacity;

    // Pad the requested extra capacity with some extra entries
    // to account for the desired load factor
    // FIXME: we don't need this here, we can simply do capacity - ht->limit to find the pad
    size_t pad = ht_calculate_limit(ht, capacity);

    if (capacity > SIZE_MAX - pad) {
        ht_unlock(ht);
        return false;
    }

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
 * Helper function to insert an entry in the hash table while
 * holding the writer mutex.
 * 
 * Note that the pointer to key and value must be stable, and
 * have a lifetime of at least the same as the hash table.
 *
 * This function will find a suitable index to insert the new
 * entry at. The caller is responsible for making sure
 * that the key does not exist in the table beforehand.
 *
 * The caller must also ensure that the table is large enough
 * to hold a new, insterted value. Ideally, the table should
 * be resized if ht->size >= ht->limit
 *
 * The caller should ensure that the writer mutex is held
 * before calling this function. Note that the returned index 
 * to the inserted entry is only guaranteed to be stable until 
 * the mutex is released.
 *
 * On success, this function returns the index to the inserted
 * entry. On failure, this function returns SIZE_MAX. Note that
 * the index is only guaranteed to be stable as long as the writer
 * mutex is held.
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

    struct ht_entry *table = atomic_load_explicit(&ht->table, memory_order_relaxed);
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
 * the writer mutex.
 *
 * Does backwards shift deletion from the specified index, 
 * in order to maintain distance from ideal (DFI).
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

    ht_lock(ht);
    size_t idx = ht_find_unlocked(ht, hash, key, key_size);
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
 *
 * Note that the pointer to key and value must be stable, and
 * have a lifetime of at least the same as the hash table.
 *
 */
static inline
bool ht_insert(struct htable *ht, uint32_t hash, 
               const void *key, size_t key_size,
               void *value)
{
    ht_lock(ht);
    size_t inserted = ht_find_unlocked(ht, hash, key, key_size);

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
    if (ht->size >= ht->limit) {
        size_t capacity = atomic_load_explicit(&ht->capacity, memory_order_relaxed);

        if (!ht_resize_unlocked(ht, capacity > 0 ? capacity * 2 : 128)) {
            ht_unlock(ht);
            return false;
        }
    }

    // Do the actual insertion
    ht_insert_unlocked(ht, hash, key, key_size, value);

    ht_unlock(ht);
    return true;
}

#ifdef __cplusplus
}
#endif
#endif
