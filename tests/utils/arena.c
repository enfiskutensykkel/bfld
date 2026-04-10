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
#define NUM_ALLOCS 10000000


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
    struct arena *arena = arena_create(NUM_ALLOCS * sizeof(int));
    struct timespec start = get_time();

    for (int i = 0; i < NUM_ALLOCS; ++i) {
        int *v = arena_alloc(arena, sizeof(int), sizeof(int));
        assert(v != NULL);
        *v = i;
    }

    struct timespec end = get_time();
    print_time(&start, &end);

    arena_destroy(arena);
}


void test_dynamic_alloc(void)
{
    struct arena_list list = {0};
    struct arena *arena = NULL;
    struct timespec start = get_time();
    
    for (int i = 0; i < NUM_ALLOCS; ++i) {
        int *v = arena_dynamic_alloc(&list, &arena,
                                     sizeof(int), sizeof(int),
                                     4096);
        assert(v != NULL);
        *v = i;
    }

    arena_dynamic_alloc_done(&list, &arena);

    struct timespec end = get_time();
    print_time(&start, &end);

    arena_list_clear(&list);
}



void test_realloc(void)
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

    struct timespec end = get_time();
    print_time(&start, &end);

    free(array);
}


int main(void)
{
    test_local_arena();
    test_realloc();
    test_dynamic_alloc();
    return 0;
}
