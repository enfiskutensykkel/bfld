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
#define NUM_ALLOCS 1000000


void print_time(const struct timespec *start, const struct timespec *end)
{
    double elapsed = (end->tv_sec - start->tv_sec) * 1000 + (end->tv_nsec - start->tv_nsec) / 1e6;
    fprintf(stderr, "test time: %.3f ms\n", elapsed);
}


struct timespec get_time(void)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return now;
}


void test_local_arena(void)
{
    struct shared_arena arena;

    shared_arena_init(&arena);
    shared_arena_reserve(&arena, NUM_ALLOCS * sizeof(int));

    struct timespec start = get_time();

    for (int i = 0; i < NUM_ALLOCS; ++i) {
        int *v = shared_arena_alloc(&arena, sizeof(int), sizeof(int));
        assert(v != NULL);
        *v = i;
    }

    struct timespec end = get_time();
    print_time(&start, &end);

    shared_arena_free(&arena);
}


void test_single_threaded_realloc(void)
{
    struct timespec start = get_time();

    int *array = NULL;

    for (int i = 0; i < NUM_ALLOCS; ++i) {
        int *a = realloc(array, sizeof(int) * (i + 1));
        assert(a != NULL);
        int *v = &a[i];
        assert(v != NULL);
        *v = i;
        array = a;
    }

    free(array);

    struct timespec end = get_time();
    print_time(&start, &end);
}


int main(void)
{
    test_local_arena();
    test_single_threaded_realloc();
    return 0;
}
