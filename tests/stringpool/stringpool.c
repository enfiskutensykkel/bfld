#include "stringpool.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>

struct string_pool pool = {0};


int main(int argc, char **argv)
{
    uint64_t offset = string_pool_intern(&pool, "hello");
    assert(strcmp(string_pool_at(&pool, offset), "hello") == 0);

    const char *strings[] = {"A", "B", "C", "this is a string", "this is another string", "A"};
    uint64_t offsets[sizeof(strings) / sizeof(strings[0])];
    for (size_t i = 0; i < sizeof(strings) / sizeof(strings[0]); ++i) {
        offsets[i] = string_pool_intern(&pool, strings[i]);
    }

    assert(offsets[0] == offsets[sizeof(strings) / sizeof(strings[0]) - 1]);

    for (size_t i = 0; i < sizeof(strings) / sizeof(strings[0]); ++i) {
        assert(string_pool_lookup(&pool, strings[i]) == offsets[i]);
    }

    string_pool_unintern(&pool, "B");

    string_pool_for_each_offset(offs, &pool) {
        fprintf(stderr, "offset=%lu string=%s\n", offs, string_pool_at(&pool, offs));
    }

    for (uint64_t i = 0; i < pool.capacity; ++i) {
        const struct intern *intern = &pool.index[i];
        if (intern->hash != 0) {
            fprintf(stderr, "offset=%lu length=%zu string=%s\n", intern->offset, intern->length, string_pool_at(&pool, intern->offset));
        }
    }

    string_pool_clear(&pool);
    assert(pool.capacity == 0);
    
    return 0;
}
