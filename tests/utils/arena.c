#include <pthread.h>
#include <assert.h>
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "arena.h"

#define NUM_THREADS 32
#define ALLOCS_PER_THREAD 100000


struct test_data
{
    struct arena *arena;
    int thread_id;
};


void * test_worker(void *arg)
{
    struct test_data *data = arg;
    int seed = data->thread_id;

    for (int i = 0; i < ALLOCS_PER_THREAD; ++i) {
        size_t size = (rand_r(&seed) % 128) + 1;
        size_t align = 1 << ((rand_r(&seed) % 4) + 3);

        void *ptr = arena_alloc(data->arena, size, align);
        //void *ptr = malloc(size);

        assert(ptr != NULL);
        assert(((uintptr_t) ptr % align) == 0);

        memset(ptr, 0xaa, size);
    }
    return NULL;
}


void test_arena_contention(void)
{
    struct arena arena = {0};

    //arena_init(&arena, 64ULL << 20, 16);

    pthread_t threads[NUM_THREADS];
    struct test_data data[NUM_THREADS];

    for (int i = 0; i < NUM_THREADS; ++i) {
        data[i].thread_id = i;
        data[i].arena = &arena;

        pthread_create(&threads[i], NULL, test_worker, &data[i]);
    }

    for (int i = 0; i < NUM_THREADS; ++i) {
        pthread_join(threads[i], NULL);
    }

    uint64_t count = 0;
    struct region *s = atomic_load(&arena.head);
    while (s != NULL) {
        ++count;
        s = atomic_load(&s->next);
    }

    fprintf(stderr, "num threads: %d\n", NUM_THREADS);
    fprintf(stderr, "allocs per thread: %d\n", ALLOCS_PER_THREAD);
    fprintf(stderr, "region size: %zu\n", arena_region_size(&arena));
    fprintf(stderr, "region align: %zu\n", arena_align(&arena));
    fprintf(stderr, "number of regions: %lu\n", count);

    arena_destroy(&arena);
}


int main(void)
{
    test_arena_contention();
    return 0;
}
