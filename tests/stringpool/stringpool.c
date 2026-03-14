#include "strpool.h"
#include "logging.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>


void test_tail_merge(void)
{
    struct strpool pool = {0};

    const char strtab[] = "\0atomic_open\0main\0domain\0_io_file_open\0tor-arne\0bjarne\0mydomain\0foobar\0bar\0file_open\0open\0arne\0per-arne";
    uint64_t totalcount = 0;
    const char *s = strtab;
    fprintf(stderr, "All strings:\n");
    while (s < strtab + sizeof(strtab)) {
        size_t n = strlen(s) + 1;
        fprintf(stderr, "'%s'\n", s);
        totalcount++;
        s += n;
    }
    strpool_pack(&pool, strtab, sizeof(strtab));

    fprintf(stderr, "\nPool entries:\n");
    strpool_for_each_offset(offs, &pool) {
        fprintf(stderr, "'%s'\n", strpool_at(&pool, offs));
    }
    assert(pool.count == totalcount);

    fprintf(stderr, "\nInterned strings:\n");
    s = &pool.strings[0];
    size_t nstrings = 0;
    size_t size = 0;
    while (s < pool.strings + pool.offset) {
        size_t n = strlen(s) + 1;
        fprintf(stderr, "'%s'\n", s);
        nstrings++;
        size += n;
        s += n;
    }
    assert(pool.offset == size);
    assert(pool.offset < sizeof(strtab));

    fprintf(stderr, "%lu\n", strpool_lookup(&pool, "arne"));
    
    strpool_clear(&pool);
}


int main(int argc, char **argv)
{
    log_level = 9;
    struct strpool pool = {0};
    uint64_t offset = strpool_intern(&pool, "hello");
    assert(strcmp(strpool_at(&pool, offset), "hello") == 0);

    const char *strings[] = {"A", "B", "C", "this is a string", "this is another string", "A"};
    uint64_t offsets[sizeof(strings) / sizeof(strings[0])];
    for (size_t i = 0; i < sizeof(strings) / sizeof(strings[0]); ++i) {
        offsets[i] = strpool_intern(&pool, strings[i]);
    }

    assert(offsets[0] == offsets[sizeof(strings) / sizeof(strings[0]) - 1]);

    for (size_t i = 0; i < sizeof(strings) / sizeof(strings[0]); ++i) {
        assert(strpool_lookup(&pool, strings[i]) == offsets[i]);
    }

    strpool_unintern(&pool, "B");

    strpool_for_each_offset(offs, &pool) {
        assert(strcmp("B", strpool_at(&pool, offs)) != 0);
    }

//    strpool_for_each_offset(offs, &pool) {
//        fprintf(stderr, "offset=%lu string=%s\n", offs, strpool_at(&pool, offs));
//    }
//
//    for (uint64_t i = 0; i < pool.capacity; ++i) {
//        const struct intern *intern = &pool.index[i];
//        if (intern->hash != 0) {
//            fprintf(stderr, "offset=%lu length=%zu string=%s\n", intern->offset, intern->length, strpool_at(&pool, intern->offset));
//        }
//    }

    strpool_clear(&pool);
    assert(pool.capacity == 0);

    test_tail_merge();
    
    return 0;
}
