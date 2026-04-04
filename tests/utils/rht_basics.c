#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include "rht.h"
#include "arena.h"
#include "hash.h"

#define SMASH_COUNT 1000000
#define NUM_ENTRIES 50000
#define LOAD_FACTOR 75

void print_time(struct timespec *start, struct timespec *end)
{
    double elapsed = (end->tv_sec - start->tv_sec) * 1000 + (end->tv_nsec - start->tv_nsec) / 1e6;
    fprintf(stderr, "test time: %.3f milliseconds\n", elapsed);
}

void test_smash(void)
{
    uint8_t load_factor_pct = 95;
    struct arena_list tabledata = {0};
    struct arena *table = NULL;

    struct rht ht;

    //size_t capacity = RHT_CAPACITY(SMASH_COUNT, load_factor_pct);
    size_t capacity = 1024;
    void *tablemem = arena_alloc_dynamic(&tabledata, &table, capacity * sizeof(struct rht_entry), 1);
    rht_init(&ht, tablemem, capacity, load_factor_pct);

    bool *inserted = calloc(SMASH_COUNT, sizeof(bool));
    int *keys = malloc(sizeof(int) * SMASH_COUNT);
    assert(keys != NULL);

    for (int i = 0; i < SMASH_COUNT; ++i) {
        int id;

        do {
            id = rand() % SMASH_COUNT;
        } while (id == 0);

        keys[i] = id;
    }

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    for (int i = 0; i < SMASH_COUNT * 2; ++i) {
        int id = keys[i % SMASH_COUNT];
        uint32_t h = hash_djb2_32(&id, sizeof(int));

        if (!inserted[id]) {
            int *kp = &keys[i % SMASH_COUNT];

            int res = rht_insert(&ht, h, kp, sizeof(int), (void*) (uintptr_t) id);
            if (res == -ENOSPC) {
                capacity = rht_capacity(&ht) * 2;
                tablemem = arena_alloc_dynamic(&tabledata, &table, capacity * sizeof(struct rht_entry), 1);
                rht_rehash(&ht, tablemem, capacity);
                res = rht_insert(&ht, h, kp, sizeof(int), (void*) (uintptr_t) id);
            }
            assert(res == 0);
            inserted[id] = true;
        } else {
            void *old = rht_remove(&ht, h, &id, sizeof(int));
            assert(old != NULL);
            assert((uintptr_t) old == (uintptr_t) id);
            inserted[id] = false;
        }

#ifndef NDEBUG
        if (i % 1000 == 0) {
            size_t expected = 0;
            for (int j = 0; j < SMASH_COUNT; ++j) {
                expected += inserted[j];
            }
            assert(rht_size(&ht) == expected);
        }
#endif
    }

    clock_gettime(CLOCK_MONOTONIC, &end);

    print_time(&start, &end);

    for (int i = 0; i < SMASH_COUNT; ++i) {
        int id = keys[i];
        uint32_t h = hash_djb2_32(&id, sizeof(int));

        void *val = rht_get(&ht, h, &id, sizeof(int));
        if (inserted[id]) {
            assert(val != NULL);
            assert((uintptr_t) val == (uintptr_t) id);
        } else {
            assert(val == NULL);
        }
    }

    size_t count = 0;
    size_t size = 0;
    struct arena *curr = tabledata.head;
    while (curr != NULL) {
        size += curr->used;
        curr = curr->next;
        ++count;
    }

    double load_factor = ((double) rht_size(&ht)) / rht_capacity(&ht);
    fprintf(stderr, "Hash table size: %zu\n", rht_size(&ht));
    fprintf(stderr, "Hash table capacity: %zu\n", rht_capacity(&ht));
    fprintf(stderr, "Desired load factor: %.3g%%\n", ht.load_factor * 1.0);
    fprintf(stderr, "Effective load factor: %.3g%%\n", load_factor * 100.0);
    fprintf(stderr, "Hash table memory size: %zu (%.3g MB)\n", size, size / (1024 * 1024.0));
    fprintf(stderr, "Number of arenas used for table memory: %zu\n", count);

    free(inserted);
    free(keys);
    arena_list_free(&tabledata);
}


void test_grow(void)
{
    struct arena_list keyarenas = {0};
    struct arena_list arenas = {0};
    struct arena *keystore = NULL;
    struct arena *tablestore = NULL;

    struct rht ht = RHT_INIT;

    for (int i = 0; i < NUM_ENTRIES; ++i) {
        char key[64];
        uintptr_t target = i;
        snprintf(key, sizeof(key), "key-%d", i);
        size_t len = strlen(key) + 1;

        void *keyptr = arena_alloc_dynamic(&keyarenas, &keystore, len, 1);
        memcpy(keyptr, key, len);

        uint32_t hash = hash_djb2_32(key, len);
        
        int status = rht_insert(&ht, hash, keyptr, len, (void*) target);
        if (status == -ENOSPC) {
            size_t capacity = rht_capacity(&ht) * 2;
            if (capacity == 0) {
                capacity = 8;
            }
            void *tablemem = arena_alloc_dynamic(&arenas, &tablestore, capacity * sizeof(struct rht_entry), 1);
            rht_rehash(&ht, tablemem, capacity);
            status = rht_insert(&ht, hash, keyptr, len, (void*) target);
        }
        assert(status == 0);
    }

    size_t count = 0;
    size_t size = 0;
    struct arena *curr = arenas.head;
    while (curr != NULL) {
        size += curr->used;
        curr = curr->next;
        ++count;
    }

    double load_factor = ((double) rht_size(&ht)) / rht_capacity(&ht);
    fprintf(stderr, "Hash table size: %zu\n", rht_size(&ht));
    fprintf(stderr, "Hash table capacity: %zu\n", rht_capacity(&ht));
    fprintf(stderr, "Desired load factor: %.3g%%\n", ht.load_factor * 1.0);
    fprintf(stderr, "Effective load factor: %.3g%%\n", load_factor * 100.0);
    fprintf(stderr, "Hash table memory size: %zu (%.3g MB)\n", size, size / (1024 * 1024.0));
    fprintf(stderr, "Number of arenas used for table memory: %zu\n", count);

    arena_list_free(&keyarenas);
    arena_list_free(&arenas);
}


void test_basics(void)
{
    struct arena_list keyarenas = {0};
    struct arena_list arenas = {0};
    struct arena *keystore = NULL;
    struct arena *tablestore = NULL;

    struct rht ht;

    size_t capacity = RHT_CAPACITY(NUM_ENTRIES, LOAD_FACTOR);
    fprintf(stderr, "Number of entries: %zu\n", NUM_ENTRIES);
    fprintf(stderr, "Needed capacity for a load factor of %u%%: %zu\n",
            LOAD_FACTOR, capacity);
    fprintf(stderr, "Size of each entry: %zu\n", sizeof(struct rht_entry));
    fprintf(stderr, "Needed size for array: %zu\n",
            capacity * sizeof(struct rht_entry));

    tablestore = arena_list_add(&arenas, capacity * sizeof(struct rht_entry), 64);
    assert(tablestore != NULL);
    struct rht_entry *table = arena_alloc(tablestore, capacity * sizeof(struct rht_entry), 1);
    assert(table != NULL);

    rht_init(&ht, table, capacity, LOAD_FACTOR);

    for (int i = 0; i < NUM_ENTRIES; ++i) {
        char key[64];
        uintptr_t target = i;
        snprintf(key, sizeof(key), "key-%d", i);
        size_t len = strlen(key) + 1;

        void *keyptr = arena_alloc_dynamic(&keyarenas, &keystore, len, 1);
        memcpy(keyptr, key, len);

        uint32_t hash = hash_djb2_32(key, len);
        
        assert(!rht_needs_rehash(&ht));

        int status = rht_insert(&ht, hash, keyptr, len, (void*) target);
        assert(status == 0);
    }    

    assert(rht_size(&ht) == NUM_ENTRIES);

    for (int i = 0; i < 200; ++i) {
        unsigned target = rand() % NUM_ENTRIES;
        char key[64];
        snprintf(key, sizeof(key), "key-%u", target);
        size_t len = strlen(key) + 1;
        uint32_t hash = hash_djb2_32(key, len);

        uintptr_t value = (uintptr_t) rht_get(&ht, hash, key, len);
        assert(value == target);
    }

    size_t count = 0;
    size_t size = 0;
    struct arena *curr = arenas.head;
    while (curr != NULL) {
        size += curr->used;
        curr = curr->next;
        ++count;
    }

    double load_factor = ((double) rht_size(&ht)) / rht_capacity(&ht);
    fprintf(stderr, "Hash table size: %zu\n", rht_size(&ht));
    fprintf(stderr, "Hash table capacity: %zu\n", rht_capacity(&ht));
    fprintf(stderr, "Desired load factor: %.3g%%\n", ht.load_factor * 1.0);
    fprintf(stderr, "Effective load factor: %.3g%%\n", load_factor * 100.0);
    fprintf(stderr, "Hash table memory size: %zu (%.3g MB)\n", size, size / (1024 * 1024.0));
    fprintf(stderr, "Number of arenas used for table memory: %zu\n", count);

    arena_list_free(&keyarenas);
    arena_list_free(&arenas);
}


int main(void)
{
    //test_basics();
    //test_grow();
    test_smash();
    return 0;
}
