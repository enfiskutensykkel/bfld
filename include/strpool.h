#ifndef BFLD_STRING_POOL_H
#define BFLD_STRING_POOL_H
#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdatomic.h>
#include <threads.h>
#include "utils/hash.h"


/*
 * A memory slab containing continuous, NUL-terminated strings.
 */
struct strslab
{
    struct strslab *next;   // pointer to next slab
    size_t size;            // size of the slab
    _Atomic size_t used;    // number of bytes used
    char data[];            // string data
};


/*
 * Interned string representation.
 * 
 * Contains the hash, distance from ideal (DFI), pointer to the 
 * underlying memory slab and offset into the slab to the actual string.
 */
struct strintern
{
    uint32_t hash;          // computed hash of the string (0 means unused)
    uint32_t dfi;           // distance from ideal (for Robin Hood hashing)
    const char *string;     // pointer to the actual (interned) string
    size_t length;          // length of the string
};


/*
 * Calculate the rehash threshold for a given capacity.
 *
 * To minimise hash collisions, the hash table needs to 
 * be rehashed when the number of strings exceed a given load factor (in this case, 75%).
 *
 * Capacity must be a power of two.
 */
#define STRING_POOL_REHASH_THRESHOLD(capacity) (((capacity) / 4) * 3)


/*
 * Minimum string slab size.
 */
#define STRING_POOL_MIN_SLAB_SIZE 1024


/* 
 * String pool implementation.
 *
 * Intern NUL-terminated strings into a global string pool, saving space
 * for identical strings and providing stable pointers to identical
 * strings (allowing quick comparison with == instead of strcmp).
 *
 * Strings are stored continuously in memory slabs.
 *
 * The string pool maintains an index of the interned strings
 * as a hash table (strings are hashed with a 32-bit FNV-1a hash).
 * This hash table uses Robin Hood hashing to deal with collision,
 * and rehashes when size >= rehash_threshold (load factor 0.75).
 */
struct strpool
{
    _Atomic int32_t rwlock;     // reader-writer lock (>0 readers, -1 writer)
    uint64_t refcnt;            // reference counter
    _Atomic(struct strslab *) head;       // pointer to the current slab
    struct strintern *index;    // string index table (hash table)
    uint64_t size;              // size of the index table (number of strings)
    uint64_t capacity;          // capacity of the index table (must be power of 2)
    uint64_t threshold;         // threshold for when to extend and rehash the index
};


static inline
void strpool_init(struct strpool *pool)
{
    atomic_init(&pool->rwlock, 0);
    pool->refcnt = 0;
    atomic_init(&pool->head, NULL);
    pool->index = NULL;
    pool->size = 0;
    pool->capacity = 0;
    pool->threshold = 0;
}


/*
 * Create a new string pool.
 */
struct strpool * strpool_alloc(void);


/*
 * Clear the index and remove all interned strings.
 *
 * Note that this will also remove all underlying slabs,
 * meaning that all pointers will be invalid.
 */
void strpool_clear(struct strpool *pool);


/*
 * Take a string pool reference.
 */
struct strpool * strpool_get(struct strpool *pool);


/*
 * Release the string pool reference.
 */
void strpool_put(struct strpool *pool);


/*
 * Take the reader lock.
 */
static inline
void strpool_rdlock(struct strpool *pool)
{
    while (true) {
        int32_t lock = atomic_load(&pool->rwlock);
        if (lock >= 0 && atomic_compare_exchange_weak(&pool->rwlock, &lock, lock + 1)) {
            return;
        }
    }
}


/*
 * Release the reader lock.
 */
static inline
void strpool_rdunlock(struct strpool *pool)
{
    atomic_fetch_sub(&pool->rwlock, 1);
}


/*
 * Take the writer lock.
 */
static inline
void strpool_wrlock(struct strpool *pool)
{
    int32_t expected = 0;

    while (!atomic_compare_exchange_weak(&pool->rwlock, &expected, -1)) {
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
 * Release the writer lock.
 */
static inline
void strpool_wrunlock(struct strpool *pool)
{
    atomic_store(&pool->rwlock, 0);
}


/*
 * Extend and rehash index.
 * The caller must take the writer lock before calling this.
 */
bool strpool_reserve_unlocked(struct strpool *pool, uint64_t capacity);


/*
 * Extend the string pool index to the new capacity, and rehash
 * all the old entries into the newly extended index.
 */
static inline
bool strpool_reserve(struct strpool *pool, uint64_t capacity)
{
    bool status;
    strpool_wrlock(pool);
    status = strpool_reserve_unlocked(pool, capacity);
    strpool_wrunlock(pool);
    return status;
}


/*
 * Helper function to appends the string to the most recent slab.
 *
 * If there is not enough space in the current slab, this will 
 * allocate a new slab try to update the pool.
 */
const char * strpool_slab_intern(struct strpool *pool, const char *string, size_t length);


/*
 * Helper function look up a string in the index.
 * The caller should take the necessary lock before calling this.
 */
static inline
const char * strpool_lookup_unlocked(struct strpool *pool, uint32_t hash, 
                                     const char *string, size_t length)
{
    if (pool->size == 0) {
        return NULL;
    }

    uint64_t slot = hash & (pool->capacity - 1);
    uint32_t dfi = 0;

    while (pool->index[slot].hash != 0 && dfi <= pool->index[slot].dfi) {
        const struct strintern *this = &pool->index[slot];

        if (this->hash == hash && this->length == length) {
            if (memcmp(string, this->string, length) == 0) {
                return this->string;
            }
        }

        slot = (slot + 1) & (pool->capacity - 1);
        ++dfi;
    }

    return NULL;
}


/*
 * Add a string to the string pool and return a stable pointer.
 * If the string is already added, the existing stable pointer is returned.
 * Returns NULL on errors, for example if memory allocation fails.
 */
static inline
const char * strpool_intern(struct strpool *pool, const char *string)
{
    size_t length = strlen(string);
    uint32_t hash = hash_fnv1a_32(string, length);
    if (hash == 0) {
        hash = 1;  // hash == 0 means unused entry
    }

    // Try to find string first
    strpool_rdlock(pool);
    const char *found = strpool_lookup_unlocked(pool, hash, string, length);
    strpool_rdunlock(pool);
    if (found != NULL) {
        return found;
    }

    // String was not found, intern it into a slab
    // There is a chance that multiple threads may simultaneously
    // intern the same string here, but the chance is low
    const char *interned = strpool_slab_intern(pool, string, length);

    strpool_wrlock(pool);

    // We need to check again if the string was already interned
    found = strpool_lookup_unlocked(pool, hash, string, length);
    if (found != NULL) {
        strpool_wrunlock(pool);
        return found;
    }

    // Check if we need to resize and rehash the index
    if (pool->size >= pool->threshold) {
        if (!strpool_reserve_unlocked(pool, pool->capacity * 2)) {
            // Unable to extend the index
            strpool_wrunlock(pool);
            return NULL;
        }
    }

    string = interned;
    uint64_t slot = hash & (pool->capacity - 1);
    uint32_t dfi = 0;

    while (hash != 0) {
        struct strintern *this = &pool->index[slot];

        if (this->hash == 0 || dfi > this->dfi) {
            struct strintern tmp = *this;

            // Update the current slot and find a new home
            // for the slot we are replacing
            this->hash = hash;
            this->length = length;
            this->string = string;
            this->dfi = dfi;

            hash = tmp.hash;
            string = tmp.string;
            dfi = tmp.dfi;
            length = tmp.length;
        }

        slot = (slot + 1) & (pool->capacity - 1);
        ++dfi;
    }

    pool->size++;
    strpool_wrunlock(pool);
    return interned;
}


/*
 * Look up a string in the pool and return a stable pointer.
 * Returns NULL if the string is not found.
 */
static inline
const char * strpool_lookup(struct strpool *pool, const char *string)
{
    size_t length = strlen(string);
    uint32_t hash = hash_fnv1a_32(string, length);
    if (hash == 0) {
        hash = 1;  // hash == 0 means unused entry
    }

    strpool_rdlock(pool);
    const char *found = strpool_lookup_unlocked(pool, hash, string, length);
    strpool_rdunlock(pool);
    return found;
}


#ifdef __cplusplus
}
#endif
#endif
