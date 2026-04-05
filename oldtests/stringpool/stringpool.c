#include "strpool.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <pthread.h>
#include "utils/align.h"


#define NUM_THREADS 32
#define NUM_STRINGS 10000

struct thread_data 
{
    struct strpool *pool;
    int thread_id;
    const char **results;
    const char *shared;
    const char **collisions;
};


void * test_worker(void *arg)
{
    struct thread_data *data = (struct thread_data*) arg;
    struct strpool *pool = data->pool;
    char buf[64];

    data->shared = strpool_intern(pool, "shared_symbol");

    for (int i = 0; i < NUM_STRINGS; ++i) {
        const char *shared = strpool_intern(pool, "shared_symbol");
        assert(strcmp(shared, "shared_symbol") == 0);
        assert(shared == data->shared);

        snprintf(buf, sizeof(buf), "unique_%d_%d", data->thread_id, i);
        data->results[i] = strpool_intern(pool, buf);

        snprintf(buf, sizeof(buf), "common_%d", rand() % 100);
        data->collisions[i] = strpool_intern(pool, buf);

        snprintf(buf, sizeof(buf), "unique_%d_%d", data->thread_id, i);
        const char *lookup = strpool_lookup(pool, buf);
        assert(lookup == data->results[i]);

    }

    return NULL;
}


void test_contention(void)
{
    const int nthreads = NUM_THREADS;
    struct strpool pool = {0};
    pthread_t threads[nthreads];
    struct thread_data data[nthreads];

    const char *results[nthreads][NUM_STRINGS];
    const char *collisions[nthreads][NUM_STRINGS];

    strpool_reserve(&pool, (1ULL << 20));

    fprintf(stderr, "slab size %llu\n", STRPOOL_SLAB_SIZE);

    for (int i = 0; i < nthreads; ++i) {
        data[i].pool = &pool;
        data[i].thread_id = i;
        data[i].results = results[i];
        data[i].shared = NULL;
        data[i].collisions = collisions[i];
        pthread_create(&threads[i], NULL, test_worker, &data[i]);
    }

    for (int i = 0; i < nthreads; ++i) {
        pthread_join(threads[i], NULL);
    }

    const char *first_shared = strpool_lookup(&pool, "shared_symbol");
    for (int i = 0; i < nthreads; ++i) {
        assert(data[i].shared == first_shared);
        
        for (int j = 0; j < NUM_STRINGS; ++j) {
            char buf[64];
            snprintf(buf, sizeof(buf), "unique_%d_%d", i, j);

            const char *found = strpool_lookup(&pool, buf);
            assert(found == results[i][j]);

        }
    }

    struct strpool unique_strings = {0};
    uint64_t duplicates = 0;
    uint64_t unique = 0;
    const struct strslab *slab = pool.head;

    while (slab != NULL && slab->used > 0) {
        const char *s = slab->data;

        while (s < slab->data + slab->used) {

            if (strpool_lookup(&unique_strings, s) != NULL) {
                ++duplicates;
            } else {
                strpool_intern(&unique_strings, s);
                ++unique;
            }

            size_t len = strlen(s);
            s += align_to(len + 1, 16);
        }

        slab = slab->next;
    }
    fprintf(stderr, "Number of unique %lu (%lu), number of duplicate %lu\n", 
            pool.size, unique, duplicates);

    strpool_clear(&unique_strings);
    strpool_clear(&pool);
}


int main(void)
{
    test_contention();
    return 0;
}


//void test_tail_merge(void)
//{
//    struct strpool pool = {0};
//
//    const char strtab[] = "\0atomic_open\0main\0domain\0_io_file_open\0tor-arne\0bjarne\0mydomain\0foobar\0bar\0file_open\0open\0arne\0per-arne";
//    uint64_t totalcount = 0;
//    const char *s = strtab;
//    fprintf(stderr, "All strings:\n");
//    while (s < strtab + sizeof(strtab)) {
//        size_t n = strlen(s) + 1;
//        fprintf(stderr, "'%s'\n", s);
//        totalcount++;
//        s += n;
//    }
//    strpool_pack(&pool, strtab, sizeof(strtab));
//
//    fprintf(stderr, "\nPool entries:\n");
//    strpool_for_each_offset(offs, &pool) {
//        fprintf(stderr, "'%s'\n", strpool_at(&pool, offs));
//    }
//    assert(pool.count == totalcount);
//
//    fprintf(stderr, "\nInterned strings:\n");
//    s = &pool.strings[0];
//    size_t nstrings = 0;
//    size_t size = 0;
//    while (s < pool.strings + pool.offset) {
//        size_t n = strlen(s) + 1;
//        fprintf(stderr, "'%s'\n", s);
//        nstrings++;
//        size += n;
//        s += n;
//    }
//    assert(pool.offset == size);
//    assert(pool.offset < sizeof(strtab));
//
//    fprintf(stderr, "%lu\n", strpool_lookup(&pool, "arne"));
//    
//    strpool_clear(&pool);
//}
//
//
//int main(int argc, char **argv)
//{
//    log_level = 9;
//    struct strpool pool = {0};
//    uint64_t offset = strpool_intern(&pool, "hello");
//    assert(strcmp(strpool_at(&pool, offset), "hello") == 0);
//
//    const char *strings[] = {"A", "B", "C", "this is a string", "this is another string", "A"};
//    uint64_t offsets[sizeof(strings) / sizeof(strings[0])];
//    for (size_t i = 0; i < sizeof(strings) / sizeof(strings[0]); ++i) {
//        offsets[i] = strpool_intern(&pool, strings[i]);
//    }
//
//    assert(offsets[0] == offsets[sizeof(strings) / sizeof(strings[0]) - 1]);
//
//    for (size_t i = 0; i < sizeof(strings) / sizeof(strings[0]); ++i) {
//        assert(strpool_lookup(&pool, strings[i]) == offsets[i]);
//    }
//
//    strpool_unintern(&pool, "B");
//
//    strpool_for_each_offset(offs, &pool) {
//        assert(strcmp("B", strpool_at(&pool, offs)) != 0);
//    }
//
////    strpool_for_each_offset(offs, &pool) {
////        fprintf(stderr, "offset=%lu string=%s\n", offs, strpool_at(&pool, offs));
////    }
////
////    for (uint64_t i = 0; i < pool.capacity; ++i) {
////        const struct intern *intern = &pool.index[i];
////        if (intern->hash != 0) {
////            fprintf(stderr, "offset=%lu length=%zu string=%s\n", intern->offset, intern->length, strpool_at(&pool, intern->offset));
////        }
////    }
//
//    strpool_clear(&pool);
//    assert(pool.capacity == 0);
//
//    test_tail_merge();
//    
//    return 0;
//}
