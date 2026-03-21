#include <pthread.h>
#include <assert.h>
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "arena.h"

#define NUM_THREADS 32
#define ALLOCS_PER_THREAD 100000

struct test_data
{
    struct arena_list *list;
    struct arena *arena;
    struct arena **cache;
    int thread_id;
    pthread_barrier_t *barrier;
    void * (*alloc)(const struct test_data*,size_t,size_t);
};


void * malloc_wrapper(const struct test_data *, size_t size, size_t align)
{
    return malloc(size);
}


void * alloc_dynamic_wrapper(const struct test_data *data, size_t size, size_t align)
{
    void *ptr = arena_alloc_dynamic(data->list, data->cache, size, align);
    assert(ptr != NULL);
    assert(((uintptr_t) ptr % align) == 0);
    return ptr;
}


void * alloc_wrapper(const struct test_data *data, size_t size, size_t align)
{
    void *ptr = arena_alloc_block_threadsafe(data->arena, size, align);
    assert(ptr != NULL);
    assert(((uintptr_t) ptr % align) == 0);
    return ptr;
}


void * test_worker(void *arg)
{
    struct test_data *data = arg;
    int seed = data->thread_id;

    pthread_barrier_wait(data->barrier);

    for (int i = 0; i < ALLOCS_PER_THREAD; ++i) {
        size_t size = (rand_r(&seed) % 128) + 1;
        size_t align = 1 << ((rand_r(&seed) % 4) + 3);

        void *ptr = data->alloc(data, size, align);
        memset(ptr, data->thread_id, size);
    }

    pthread_barrier_wait(data->barrier);

    int *ptr = data->alloc(data, sizeof(int), 4);
    *ptr = data->thread_id;

    pthread_barrier_wait(data->barrier);

    return NULL;
}


void print_time(struct timespec *start, struct timespec *end)
{
    double elapsed = (end->tv_sec - start->tv_sec) + (end->tv_nsec - start->tv_nsec) / 1e9;
    fprintf(stderr, "test time: %.3f seconds\n", elapsed);
}


void print_list(struct arena_list *list)
{
    uint64_t count = 0;
    uint64_t total = 0;
    struct arena *a = atomic_load(&list->head);
    while (a != NULL) {
        ++count;
        total += a->size;
        a = atomic_load(&a->next);
    }

    fprintf(stderr, "num threads: %d\n", NUM_THREADS);
    fprintf(stderr, "allocs per thread: %d\n", ALLOCS_PER_THREAD);
    fprintf(stderr, "size per arena: %zu\n", arena_size(list));
    fprintf(stderr, "number of arenas: %lu\n", count);
    fprintf(stderr, "total size: %zu\n", total);
}


void test_arena_contention(void)
{

    struct arena_list list = {0};

    pthread_barrier_t barrier;
    pthread_t threads[NUM_THREADS];
    struct test_data data[NUM_THREADS];

    struct timespec start, end;

    struct arena *arena = arena_create(&list, 128 * NUM_THREADS * ALLOCS_PER_THREAD + sizeof(int) * NUM_THREADS);

    pthread_barrier_init(&barrier, NULL, NUM_THREADS + 1);

    for (int i = 0; i < NUM_THREADS; ++i) {
        data[i].thread_id = i;
        data[i].list = &list;
        data[i].cache = NULL;
        data[i].arena = arena;
        data[i].barrier = &barrier;
        data[i].alloc = alloc_wrapper;

        pthread_create(&threads[i], NULL, test_worker, &data[i]);
    }

    pthread_barrier_wait(&barrier);
    clock_gettime(CLOCK_MONOTONIC, &start);
    pthread_barrier_wait(&barrier);
    pthread_barrier_wait(data->barrier);
    clock_gettime(CLOCK_MONOTONIC, &end);

    for (int i = 0; i < NUM_THREADS; ++i) {
        pthread_join(threads[i], NULL);
    }

    fprintf(stderr, "test_arena_contention\n");
    print_time(&start, &end);
    print_list(&list);

    pthread_barrier_destroy(&barrier);
    arena_destroy(&list);
}



void test_dynamic(void)
{
    struct arena_list list = {0};

    pthread_barrier_t barrier;
    pthread_t threads[NUM_THREADS];
    struct test_data data[NUM_THREADS];
    struct arena *cache[NUM_THREADS];
    struct timespec start, end;

    pthread_barrier_init(&barrier, NULL, NUM_THREADS + 1);

    for (int i = 0; i < NUM_THREADS; ++i) {
        cache[i] = NULL;
        data[i].arena = NULL;
        data[i].thread_id = i;
        data[i].list = &list;
        data[i].cache = &cache[i];
        data[i].barrier = &barrier;
        data[i].alloc = alloc_dynamic_wrapper;

        pthread_create(&threads[i], NULL, test_worker, &data[i]);
    }

    pthread_barrier_wait(&barrier);
    clock_gettime(CLOCK_MONOTONIC, &start);
    pthread_barrier_wait(&barrier);
    pthread_barrier_wait(data->barrier);
    clock_gettime(CLOCK_MONOTONIC, &end);

    for (int i = 0; i < NUM_THREADS; ++i) {
        pthread_join(threads[i], NULL);
    }

    fprintf(stderr, "test_dynamic\n");
    print_time(&start, &end);
    print_list(&list);

    pthread_barrier_destroy(&barrier);
    arena_destroy(&list);
}


int main(void)
{
    test_arena_contention();
    fprintf(stderr, "\n");
    test_dynamic();
    return 0;
}
