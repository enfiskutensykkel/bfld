#include <pthread.h>
#include <assert.h>
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "arena.h"

#define NUM_THREADS 16
#define ALLOCS_PER_THREAD 100000


struct test_data
{
    struct arena *arena;
    int thread_id;
};


void * test_worker(void *arg)
{
    struct test_data *data = arg;

    for (int i = 0; i < ALLOCS_PER_THREAD; ++i) {
        size_t size = (rand() % 128) + 1;
        size_t align = 1 << ((rand() % 4) + 3);

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
    struct slab *s = atomic_load(&arena.head);
    while (s != NULL) {
        ++count;
        s = s->next;
    }

    fprintf(stderr, "num threads: %d\n", NUM_THREADS);
    fprintf(stderr, "allocs per thread: %d\n", ALLOCS_PER_THREAD);
    fprintf(stderr, "slab min size: %zu\n", arena_slab_size(&arena));
    fprintf(stderr, "slab min align: %zu\n", arena_slab_align(&arena));
    fprintf(stderr, "number of slabs: %lu\n", count);

    arena_destroy(&arena);
}


int main(void)
{
    test_arena_contention();
    return 0;
}
