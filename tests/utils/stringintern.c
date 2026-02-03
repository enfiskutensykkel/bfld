#include "stringintern.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

struct strings pool = {0};


int main(int argc, char **argv)
{
    uint64_t offset = strings_intern(&pool, "hello");
    assert(strcmp(strings_at(&pool, offset), "hello") == 0);

    const char *strings[] = {"A", "B", "C", "this is a string", "this is another string", "A"};
    uint64_t offsets[sizeof(strings) / sizeof(strings[0])];
    for (size_t i = 0; i < sizeof(strings) / sizeof(strings[0]); ++i) {
        offsets[i] = strings_intern(&pool, strings[i]);
    }

    assert(offsets[0] == offsets[sizeof(strings) / sizeof(strings[0]) - 1]);

    for (size_t i = 0; i < sizeof(strings) / sizeof(strings[0]); ++i) {
        assert(strings_lookup(&pool, strings[i]) == offsets[i]);
    }

    strings_clear(&pool);
    assert(pool.table_capacity == 0);

    return 0;
}
