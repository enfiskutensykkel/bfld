#ifndef BFLD_FAST_HASH_TABLE_H
#define BFLD_FAST_HASH_TABLE_H
#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <string.h>
#include <stdlib.h>
#include "align.h"
#include "cdefs.h"
#include "spinlock.h"


struct htable_node;


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
 * are stable, and pointing to memory that that has a life time at least
 * as long as the hash table.
 */
struct htable
{
    _Atomic(struct htable_node*) *slots;
    size_t capacity;
    atomic_size_t size;
    struct rwlock lock;
};


/*
 * Hash table node.
 * 
 * Contains a key and the size of the key.
 *
 * Comparisons for keys are done as follows,
 * - first the size is checked
 * - if the sizes are equal, check if the pointers are equal
 * - if the sizes are equal and pointers are not the same value,
 *   the value pointed to by the pointers are compared with memcmp.
 */
struct htable_node
{
    const void *key;
    size_t size;
};


/*
 * Get the containing struct (the entry).
 */
#define htable_entry(node_ptr, type, member) containerof(node_ptr, type, member)


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
 * Initialize the hash table with at least the given capacity.
 * Note that capacity should be large enough to avoid a high
 * load factor, ideally 2-4 times as large as the expected
 * number of entries.
 */
void htable_init(struct htable *ht, size_t capacity);


/*
 * Free the memory allocated by the hash table.
 * Note that this is not thread-safe; this function should
 * only be called once it is guaranteed that there are no
 * more threads that are accessing the table.
 */
void htable_free(struct htable *ht);


/*
 * Search for a node in the hash table from its associated key.
 *
 * Returns the htable_node if an entry with the same key was found
 * in the table, or NULL otherwise.
 */
static inline
struct htable_node * htable_get(const struct htable *ht, uint64_t hash,
                                const void *key, size_t size)
{

    _Atomic(struct htable_node*) *slots = ht->slots;
    struct htable_node *slot;

    for (size_t idx = hash & (ht->capacity - 1);
            (slot = atomic_load_explicit(&slots[idx], memory_order_acquire)) != NULL;
            idx = (idx + 1) & (ht->capacity - 1)) {

        if (likely(slot->size == size)) {
            if (slot->key == key || memcmp(slot->key, key, size) == 0) {
                return slot;
            }
        }
    }

    return NULL;
}


/*
 * Insert a node into the hash table.
 * 
 * Tries to find an empty slot for the given key in the hash table.
 * If an empty slot is found, the entry is inserted into the table and
 * node is returned. If an entry with the same key already exists in
 * the table, the already inserted node is returned.
 *
 * In effect, this is the same as a get-or-put function, as the node
 * is inserted if the key is not already found in the hash table.
 *
 * Note that the pointers to key value must be stable and point
 * to memory with a lifetime that is guaranteed to be at least
 * as long as the lifetime of the hash table.
 */
static inline
struct htable_node * htable_put(struct htable *ht, uint64_t hash,
                                const void *key, size_t size,
                                struct htable_node *node)
{
    if (unlikely(htable_size(ht) >= ht->capacity - (ht->capacity >> 4))) {
        // Table is more than ~90% full don't try to insert anything
        return htable_get(ht, hash, key, size);
    }

    size_t idx = hash & (ht->capacity - 1);

    node->key = key;
    node->size = size;
    
    for (;;) {
        struct htable_node *slot = NULL;
        
        // Try to insert node atomically
        if (atomic_compare_exchange_strong_explicit(&ht->slots[idx], &slot, node,
                                                    memory_order_release,
                                                    memory_order_acquire)) {
            atomic_fetch_add_explicit(&ht->size, 1, memory_order_release);
            return node;
        }

        // Insertion failed, check if the key already eists
        if (likely(slot->size == size)) {
            if (slot->key == key || memcmp(slot->key, key, size) == 0) {
                return slot;
            }
        }

        idx = (idx + 1) & (ht->capacity - 1);
    }
}


/*
 * A variant of htable_put() that returns true if the
 * key didn't already exist in the hash table, and the
 * node was inserted.
 */
static inline
bool htable_put_check(struct htable *ht, uint64_t hash,
                      const void *key, size_t key_size,
                      struct htable_node *node)
{
    return htable_put(ht, hash, key, key_size, node) == node;
}


#ifdef __cplusplus
}
#endif
#endif
