#include "logging.h"
#include "utils/rbtree.h"
#include "stringpool.h"
#include "utils/align.h"
#include "utils/hash.h"
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>


struct string
{
    struct rb_node node;
    uint64_t offset;
    size_t length;
};


static inline
int strrevcmp(const char *s1, size_t len1, const char *s2, size_t len2)
{
    size_t i = len1 > 0 ? len1 - 1 : 0;
    size_t j = len2 > 0 ? len2 - 1 : 0;

    while (i != 0 && j != 0) {
        unsigned char c1 = s1[i - 1];
        unsigned char c2 = s2[j - 1];

        if (c1 != c2) {
            return c1 - c2;
        }

        --i, --j;
    }

    return (int) len1 - (int) len2;
}


static inline
bool strissuffix(const char *suffix, size_t suffixlen, const char *s, size_t len)
{
    if (suffixlen > len) {
        return false;
    }

    return memcmp(suffix, s + (len - suffixlen), suffixlen) == 0;
}


static struct string * tree_add_string(struct rb_tree *tree, const char *strings, uint64_t offset, size_t length)
{
    struct string *w = malloc(sizeof(struct string));
    if (w == NULL) {
        return NULL;
    }

    w->offset = offset;
    w->length = length;  // includes the NUL-character

    struct rb_node **pos = &(tree->root), *parent = NULL;
    const char *string = &strings[offset];

    while (*pos != NULL) {
        struct string *this = rb_entry(*pos, struct string, node);
        parent = *pos;

        int result = strrevcmp(string, length, &strings[this->offset], this->length);
        if (result < 0) {
            pos = &((*pos)->left);
        } else if (result > 0) {
            pos = &((*pos)->right);
        } else {
            free(w);
            return this;
        }
    }

    rb_insert_node(&w->node, parent, pos);
    rb_insert_fixup(tree, &w->node);
    return w;
}


struct string_pool * string_pool_alloc(void)
{
    struct string_pool *pool = malloc(sizeof(struct string_pool));
    if (pool == NULL) {
        return NULL;
    }

    pool->refcnt = 1;
    pool->strings = NULL;
    pool->offset = 0;
    pool->size = 0;
    pool->count = 0;
    pool->capacity = 0;
    pool->index = NULL;
    pool->rehash_threshold = 0;

    string_pool_extend(pool, 256);
    string_pool_rehash(pool, 64);
    return pool;
}


struct string_pool * string_pool_get(struct string_pool *pool)
{
    assert(pool != NULL);
    assert(pool->refcnt != 0);
    pool->refcnt++;
    return pool;
}


void string_pool_put(struct string_pool *pool)
{
    assert(pool != NULL);
    assert(pool->refcnt != 0);

    if (--(pool->refcnt) == 0) {
        string_pool_clear(pool);
        free(pool);
    }
}



bool string_pool_rehash(struct string_pool *pool, uint64_t capacity)
{
    if (capacity < 64) {
        capacity = 64;
    }

    if (capacity <= pool->capacity) {
        return true;
    }

    // Make sure capacity is aligned to a power of two
    capacity = align_roundup(capacity);

    // Naive check for overflow
    if (capacity * sizeof(struct intern) < pool->capacity * sizeof(struct intern)) {
        return false;
    }

    struct intern *index = (struct intern*) calloc(capacity, sizeof(struct intern));
    if (index == NULL) {
        return false;
    }

    for (uint64_t i = 0; i < pool->capacity; ++i) {
        const struct intern *entry = &pool->index[i];

        if (entry->hash == 0) {
            continue;
}

        uint32_t hash = entry->hash;
        uint32_t dfi = 0;
        uint64_t offset = entry->offset;
        size_t length = entry->length;
        uint64_t slot = hash & (capacity - 1);
        
        while (hash != 0) {
            struct intern *this = &index[slot];

            if (this->hash == 0 || dfi > this->dfi) {
                uint64_t tmp_offset = this->offset;
                uint32_t tmp_hash = this->hash;
                uint32_t tmp_dfi = this->dfi;
                size_t tmp_length = this->length;

                this->hash = hash;
                this->dfi = dfi;
                this->offset = offset;
                this->length = length;

                offset = tmp_offset;
                hash = tmp_hash;
                dfi = tmp_dfi;
                length = tmp_length;
            }

            slot = (slot + 1) & (capacity - 1);
            ++dfi;
        }
    }

    free(pool->index);
    pool->index = index;
    pool->capacity = capacity;
    pool->rehash_threshold = STRING_POOL_REHASH_THRESHOLD(capacity);

    if (pool->strings == NULL) {
        if (string_pool_extend(pool, 256)) {
            pool->strings[0] = '\0';
            pool->offset = 1;
        }
    }

    return true;
}


bool string_pool_extend(struct string_pool *pool, size_t length)
{
    if (pool->size - pool->offset >= length) {
        return true;
    }

    // Naive check for overflow
    if (pool->offset + length < pool->offset) {
        return false;
    }

    uint64_t size = pool->size > 0 ? pool->size * 2 : 256;
    if (size < pool->offset + length) {
        size = align_pow2(pool->offset + length);
    }

    char *strings = (char*) realloc(pool->strings, size);
    if (strings == NULL) {
        return false;
    }

    if (pool->strings == NULL && pool->offset == 0) {
        strings[0] = '\0';
        pool->offset = 1;
    }

    pool->strings = strings;
    pool->size = size;
    return true;
}


void string_pool_clear(struct string_pool *pool)
{
    if (pool->index != NULL) {
        free(pool->index);
    }
    pool->index = NULL;
    pool->capacity = 0;
    pool->count = 0;
    pool->rehash_threshold = 0;

    if (pool->strings != NULL) {
        free(pool->strings);
    }
    pool->strings = NULL;
    pool->size = 0;
    pool->offset = 0;
}


static bool string_pool_insert_tail_merge_offset(struct string_pool *pool, uint64_t base_offset, size_t base_length, size_t tail_length)
{
    size_t length = tail_length;
    uint64_t relative_offset = base_length - tail_length;
    uint64_t offset = base_offset + relative_offset;

    uint32_t hash = hash_fnv1a_32(&pool->strings[offset], length);
    if (hash == 0) {
        hash = 1;
    }

    uint64_t mask = pool->capacity - 1;
    uint64_t slot = hash & mask;
    uint32_t dfi = 0;

    while (hash != 0) {
        struct intern *this = &pool->index[slot];

        if (this->hash == 0 || dfi > this->dfi) {
            uint32_t tmp_hash = this->hash;
            uint32_t tmp_dfi = this->dfi;
            uint64_t tmp_offset = this->offset;
            size_t tmp_length = this->length;

            this->hash = hash;
            this->offset = offset;
            this->length = length;
            this->dfi = dfi;

            hash = tmp_hash;
            offset = tmp_offset;
            dfi = tmp_dfi;
            length = tmp_length;
        }

        slot = (slot + 1) & mask;
        ++dfi;
    }

    pool->count++;

    return false;
}



struct string_pool * string_pool_clone_and_compact(const struct string_pool *pool)
{
    struct rb_tree tree;
    struct rb_node *node;
    const char *strings = pool->strings;

    struct string_pool *clone = NULL;

    rb_tree_init(&tree);

    for (uint64_t i = 0; i < pool->capacity; ++i) {
        const struct intern *this = &pool->index[i];
        if (this->hash != 0) {
            struct string *s = tree_add_string(&tree, strings, this->offset, this->length);
            if (s == NULL) {
                goto leave;
            }
        }
    }

    clone = string_pool_alloc();
    if (clone == NULL) {
        goto leave;
    }

    string_pool_extend(clone, pool->size);
    string_pool_rehash(clone, pool->capacity);

    node = rb_last(&tree);
    struct string *longest = NULL;
    uint64_t offset = 0;

    while (node != NULL) {
        struct string *s = rb_entry(node, struct string, node);

        if (longest == NULL) {
            offset = string_pool_intern(clone, &strings[s->offset]);
            longest = s;

        } else if (!strissuffix(&strings[s->offset], s->length, &strings[longest->offset], longest->length)) {
            offset = string_pool_intern(clone, &strings[s->offset]);
            longest = s;

        } else {
            string_pool_insert_tail_merge_offset(clone, offset, longest->length, s->length);
        }

        node = rb_prev(node);
    }

leave:
    while (tree.root != NULL) {
        struct rb_node *node = tree.root;
        struct string *s = rb_entry(node, struct string, node);
        rb_remove(&tree, node);
        free(s);
    }

    return clone;
}
