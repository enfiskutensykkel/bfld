#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "htable.h"
#include "arena.h"
#include "hash.h"

#define NUM_ENTRIES 50000

struct arena_list keystore = {0};
struct arena_list arenas = {0};

struct htable ht = {0};


size_t print_arena_info(struct arena *head)
{
    if (head == NULL) {
        return 0;
    }

    size_t size = head->size;
    struct arena *next = atomic_load(&head->next);
    if (next) {
        size += print_arena_info(next);
    }

    size_t used = atomic_load(&head->used);
    
    char sizebuf[128];
    snprintf(sizebuf, sizeof(sizebuf), "%10zu", head->size);

    char usedbuf[128];
    snprintf(usedbuf, sizeof(usedbuf), "%10zu", used);

    fprintf(stderr, "%10s   %10s   %10.2f%%\n",
            sizebuf, usedbuf, ((double) used / (double) head->size) * 100.0);


    return size;
}


int main(void)
{
    struct arena *arena = NULL;

    arena_list_init(&keystore, 64 << 10);
    arena_list_init(&arenas, 128 << 10);
    //arena_list_init(&arenas, 128 << 10);
    //arena_list_init(&arenas, 1UL << 20);

    ht_init(&ht, &arenas, 20);
    ht_reserve(&ht, NUM_ENTRIES);

    for (int i = 0; i < NUM_ENTRIES; ++i) {
        char key[64];
        uintptr_t target = i;
        snprintf(key, sizeof(key), "key-%d", i);
        size_t len = strlen(key) + 1;

        struct arena *was = arena;
        void *keyptr = arena_alloc_dynamic(&keystore, &arena, len, 1);
        memcpy(keyptr, key, len);

        uint32_t hash = hash_djb2_32(key, len);
        bool status = ht_insert(&ht, hash, keyptr, len, (void*) target);
        assert(status == true);
    }    

    assert(ht.size == NUM_ENTRIES);

    for (int i = 0; i < 20; ++i) {
        unsigned target = rand() % NUM_ENTRIES;
        char key[64];
        snprintf(key, sizeof(key), "key-%u", target);
        size_t len = strlen(key) + 1;
        uint32_t hash = hash_djb2_32(key, len);

        uintptr_t value = (uintptr_t) ht_get(&ht, hash, key, len);
        assert(value == target);
    }

    fprintf(stderr, "**** HASH TABLE ARENAS ****\n");
    fprintf(stderr, "%10s   %10s   %11s\n", "ALLOC'D", "USED", "UTILIZATION");
    size_t size = print_arena_info(atomic_load(&arenas.head));

    fprintf(stderr, "\n**** KEY STORE ARENAS ****\n");
    fprintf(stderr, "%10s   %10s   %11s\n", "ALLOC'D", "USED", "UTILIZATION");
    size_t keysize = print_arena_info(atomic_load(&keystore.head));

    fprintf(stderr, "\n**** SUMMARY ****\n");
    double load_factor = ((double) ht_size(&ht)) / ht_capacity(&ht);
    fprintf(stderr, "Hash table size: %zu\n", ht_size(&ht));
    fprintf(stderr, "Hash table capacity: %zu\n", ht_capacity(&ht));
    fprintf(stderr, "Desired load factor: %.3g%%\n", ht.load_factor * 1.0);
    fprintf(stderr, "Effective load factor: %.3g%%\n", load_factor * 100.0);
    fprintf(stderr, "Hash table memory size: %zu (%.3g MB)\n", size, size / (1024 * 1024.0));
    fprintf(stderr, "Default arena size: %zu (%.3g MB)\n", arena_size(&arenas), arena_size(&arenas) / (1024 * 1024.0));

    ht_clear(&ht);
    arena_destroy(&arenas);
    arena_destroy(&keystore);
    return 0;
}
