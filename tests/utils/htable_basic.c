#include "htable.h"
#include "arena.h"
#include "hash.h"
#include <assert.h>
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#define NUM_ENTRIES 50000

struct arena_list arenas = {0};

struct htable ht = {0};

int main(void)
{
    ht_init(&ht, &arenas);
    ht_reserve(&ht, NUM_ENTRIES);

    for (int i = 0; i < NUM_ENTRIES; ++i) {
        char key[64];
        snprintf(key, sizeof(key), "key-%d", i);
        uintptr_t target = i;
        size_t len = strlen(key) + 1;
        uint32_t hash = hash_djb2_32(key, len);
        bool status = ht_insert_copy_key(&ht, hash, key, len, 1, (void*) target);
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

    double load_factor = ((double) ht_size(&ht)) / ht_capacity(&ht);
    fprintf(stderr, "size: %zu\n", ht_size(&ht));
    fprintf(stderr, "capacity: %zu\n", ht_capacity(&ht));
    fprintf(stderr, "load factor: %.3g\n", load_factor);

    size_t narenas = 0;
    size_t size = 0;
    struct arena *head = arenas.head;
    while (head != NULL) {
        struct arena *next = head->next;
        ++narenas;
        size += head->size;
        fprintf(stderr, "Arena size %zu\n", head->size);
        head = next;
    }

    fprintf(stderr, "Number of arenas: %zu\n", narenas);
    fprintf(stderr, "Total size: %zu\n", size);

    ht_clear(&ht);
    arena_destroy(&arenas);
    return 0;
}
