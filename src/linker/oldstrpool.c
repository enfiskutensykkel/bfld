#include "logging.h"
#include "strpool.h"
#include "utils/align.h"
#include "utils/hash.h"
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>


static inline
int strrevcmp(const char *s1, size_t len1, const char *s2, size_t len2)
{
    size_t i = len1;
    size_t j = len2;

    while (i != 0 && j != 0) {
        unsigned char c1 = s1[--i];
        unsigned char c2 = s2[--j];

        if (c1 != c2) {
            return (c1 < c2) ? -1 : 1;
        }
    }

    if (len1 < len2) {
        return -1;
    } else if (len1 > len2) {
        return 1;
    }

    return 0;
}


static inline
bool strissuffix(const char *suffix, size_t suffixlen, const char *s, size_t len)
{
    if (suffixlen > len) {
        return false;
    }

    return memcmp(suffix, s + (len - suffixlen), suffixlen) == 0;
}


struct strpool * strpool_alloc(void)
{
    struct strpool *pool = malloc(sizeof(struct strpool));
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

    strpool_extend(pool, 256);
    strpool_rehash(pool, 64);
    return pool;
}


struct strpool * strpool_get(struct strpool *pool)
{
    assert(pool != NULL);
    assert(pool->refcnt != 0);
    pool->refcnt++;
    return pool;
}


void strpool_put(struct strpool *pool)
{
    assert(pool != NULL);
    assert(pool->refcnt != 0);

    if (--(pool->refcnt) == 0) {
        strpool_clear(pool);
        free(pool);
    }
}


bool strpool_rehash(struct strpool *pool, uint64_t capacity)
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
    if (capacity * sizeof(struct strintern) < pool->capacity * sizeof(struct strintern)) {
        return false;
    }

    struct strintern *index = (struct strintern*) calloc(capacity, sizeof(struct strintern));
    if (index == NULL) {
        return false;
    }

    for (uint64_t i = 0; i < pool->capacity; ++i) {
        const struct strintern *entry = &pool->index[i];

        if (entry->hash == 0) {
            continue;
        }

        uint32_t hash = entry->hash;
        uint32_t dfi = 0;
        uint64_t offset = entry->offset;
        size_t length = entry->length;
        uint64_t slot = hash & (capacity - 1);
        
        while (hash != 0) {
            struct strintern *this = &index[slot];

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
        if (strpool_extend(pool, 256)) {
            pool->strings[0] = '\0';
            pool->offset = 1;
        }
    }

    return true;
}


bool strpool_extend(struct strpool *pool, size_t length)
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


void strpool_clear(struct strpool *pool)
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


static bool insert_tail_merge_offset(struct strpool *pool, uint64_t base_offset, size_t base_length, size_t tail_length)
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
        struct strintern *this = &pool->index[slot];

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


static inline
void swap_strings(const char **strings, size_t *lengths, uint64_t i, uint64_t j)
{
    const char *tmp_string = strings[i];
    size_t tmp_length = lengths[i];
    strings[i] = strings[j];
    lengths[i] = lengths[j];
    strings[j] = tmp_string;
    lengths[j] = tmp_length;
}


static inline
uint64_t partition_strings(const char **strings, size_t *lengths, 
                       uint64_t low, uint64_t high)
{
    // Do Sedgewick's "median of three"
    uint64_t mid = (low + high) / 2;
    if (strrevcmp(strings[mid], lengths[mid], strings[low], lengths[low]) < 0) {
        swap_strings(strings, lengths, mid, low);
    }

    if (strrevcmp(strings[high], lengths[high], strings[low], lengths[low]) < 0) {
        swap_strings(strings, lengths, low, high);
    }

    if (strrevcmp(strings[mid], lengths[mid], strings[high], lengths[high]) < 0) {
        swap_strings(strings, lengths, mid, high);
    }

    // Select middle element as pivot
    const char *pivot_s = strings[mid];
    size_t pivot_l = lengths[mid];

    // Do Hoare's partitioning
    uint64_t i = low - 1, j = high + 1;
    while (true) {
        do {
            ++i;
        } while (strrevcmp(strings[i], lengths[i], pivot_s, pivot_l) < 0);

        do {
            --j;
        } while (strrevcmp(strings[j], lengths[j], pivot_s, pivot_l) > 0);

        if (i >= j) {
            return j;
        }

        swap_strings(strings, lengths, i, j);
    }
}


static void sort_strings(const char **strings, size_t *lengths, uint64_t low, uint64_t high)
{
    if (low < high) {
        uint64_t pivot = partition_strings(strings, lengths, low, high);
        sort_strings(strings, lengths, low, pivot);
        sort_strings(strings, lengths, pivot + 1, high);
    }
}


uint64_t strpool_pack(struct strpool *pool, const char *strtab, uint64_t size)
{
    if (pool->size - pool->offset <= size) {
        if (!strpool_extend(pool, size)) {
            return 0;
        }
    }

    uint64_t count = 0;
    const char **sorted = malloc(sizeof(const char*) * size);
    size_t *lengths = malloc(sizeof(size_t) * size);

    // Extract strings and their lengths from the string table
    uint64_t pos = 0;
    while (pos < size) {
        size_t n = 0;

        while (pos + n < size && strtab[pos + n] != '\0') {
            ++n;
        }

        sorted[count] = &strtab[pos];
        lengths[count] = n;
        ++count;

        pos += n + 1;
    }

    // Sort the string table (reverse)
    sort_strings(sorted, lengths, 0, count-1);

    // Iterate from back and intern the logest version
    // of the string and add entries for substrings
    const char *longest = NULL;
    size_t length = 0;
    uint64_t offset = 0;

    for (uint64_t i = count; i > 0; --i) {
        const char *str = sorted[i - 1];
        size_t len = lengths[i - 1];

        if (longest == NULL || !strissuffix(str, len, longest, length)) {
            offset = strpool_intern(pool, str);
            longest = str;
            length = len;
        } else {
            insert_tail_merge_offset(pool, offset, length, len);
        }
    }
    
    free(lengths);
    free(sorted);
    return count;
}
