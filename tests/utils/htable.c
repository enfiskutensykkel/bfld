#include "htable.h"
#include "arena.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <assert.h>
#include <pthread.h>
#include <time.h>
#include "hash.h"


#define NUM_THREADS 8
#define NUM_SYMBOLS 1000000
#define MULTIPLIER 2


struct thread_data
{
    struct arena_list *arenas;
    struct htable *symbol_table;
    int thread_id;
    int count;
    int lost_races;
    int already;
};


struct symbol
{
    const char *name;
    int value;
    struct htable_node hnode;
};



static char *strings = NULL;

static size_t *offsets = NULL;


uint64_t hashfn(const char *name, size_t length)
{
    return hash_xxh_32(name, length);
}


/*
 * A deliberately bad hash function in order to force collisions.
 */
uint64_t bad_hashfn(const char *name, size_t length) 
{
    uint32_t h = 0;

    for(size_t i = 0; i < (length < 16 ? length : 16); i++) {
        h = h * 31 + name[i];
    }

    return h;
}


void print_time(struct timespec *start, struct timespec *end)
{
    double elapsed = (end->tv_sec - start->tv_sec) * 1000 + (end->tv_nsec - start->tv_nsec) / 1e6;
    fprintf(stderr, "Test time: %.3f milliseconds\n", elapsed);
}


void htable_diagnose(const struct htable *ht) 
{
    size_t total_probes = 0;
    size_t max_probe = 0;
    size_t filled_slots = 0;

    size_t distance_counts[512] = {0};

    for (size_t i = 0; i < ht->capacity; i++) {
        const struct htable_node *slot = atomic_load_explicit(&ht->slots[i], memory_order_relaxed);

        if (slot == NULL) {
            continue;
        }

        uint64_t hash = hashfn(slot->key, slot->size);

        filled_slots++;
        size_t ideal_idx = hash & (ht->capacity - 1);
        size_t actual_idx = i;

        size_t distance;
        if (actual_idx >= ideal_idx) {
            distance = actual_idx - ideal_idx;
        } else {
            distance = (ht->capacity - ideal_idx) + actual_idx;
        }

        total_probes += distance;
        if (distance > max_probe) {
            max_probe = distance;
        }

        if (distance < 511) {
            distance_counts[distance]++;
        } else {
            distance_counts[511]++;
        }
    }

    double load_factor = (double) filled_slots / ht->capacity;
    double avg_probe = filled_slots > 0 ? (double)total_probes / filled_slots : 0;

    fprintf(stderr, "\n--- Hash Table Diagnosis ---\n");
    fprintf(stderr, "Capacity   : %zu\n", ht->capacity);
    fprintf(stderr, "Filled     : %zu\n", filled_slots);
    fprintf(stderr, "Load Factor: %.2f%%\n", load_factor * 100.0);
    fprintf(stderr, "Avg Probe  : %.2f hops\n", avg_probe);
    fprintf(stderr, "Max Probe  : %zu hops\n", max_probe);
    fprintf(stderr, "----------------------------\n");

    int ps[] = {50, 75, 80, 90, 95, 99};
    int percentiles[sizeof(ps) / sizeof(int)] = {0};
    for (int p = 0; p < sizeof(ps) / sizeof(int); ++p) {
        percentiles[p] = -1;
    }

    size_t cumulative = 0;
    for (int i = 0; i < 512; ++i) {
        cumulative += distance_counts[i];
        
        for (int p = 0; p < sizeof(ps) / sizeof(ps[0]); ++p) {
            double pe = ((double) ps[p] / 100.0);
            if (percentiles[p] == -1 && cumulative >= (filled_slots * pe)) {
                percentiles[p] = i;
            }
        }
    }

    for (int p = 0; p < sizeof(ps) / sizeof(ps[0]); ++p) {
        fprintf(stderr, "P%02d        : %d hops\n", ps[p], percentiles[p]);
    }

    fprintf(stderr, "----------------------------\n");
}


void arena_diagnose(const struct arena_list *arenas)
{
    struct arena *curr = atomic_load(&arenas->head);
    size_t total_size = 0;
    size_t total_used = 0;
    size_t count = 0;
    while (curr != NULL) {
        count++;
        total_size += curr->size;
        total_used += atomic_load(&curr->used);
        curr = atomic_load(&curr->next);
    }

    double utilization = ((double) total_used / (double) total_size) * 100.0;

    const char *units[] = {
        "B", "kB", "MB", "GB", "TB"
    };
    int unit = 0;
    double used = total_used;
    double size = total_size;
    while (used > 1024.0) {
        ++unit;
        used /= 1024.0;
        size /= 1024.0;
    }

    fprintf(stderr, "\n--- Arena Diagnosis --------\n");
    fprintf(stderr, "Num arenas       : %zu\n", count);
    fprintf(stderr, "Total allocated  : %.2f %s\n", size, units[unit]);
    fprintf(stderr, "Total used       : %.2f %s\n", used, units[unit]);
    fprintf(stderr, "Total utilization: %.3f%%\n", utilization);
    fprintf(stderr, "----------------------------\n");
#if 0
    if (count <= 16) {
        curr = atomic_load(&arenas->head);
        size_t n = 0;
        while (curr != NULL) {
            size_t used = atomic_load(&curr->used);
            fprintf(stderr, "Arena %zu: size=%zu, used=%zu, utilization=%.2f%%\n",
                    n++, curr->size, used,
                    ((double) used / (double) curr->size) * 100.0);
            curr = atomic_load(&curr->next);
        }

        fprintf(stderr, "----------------------------\n");
    }
#endif
}


void * stress_worker(void *arg)
{
    struct thread_data *data = (struct thread_data*) arg;
    int thread_id = data->thread_id;
    
    // format "_ZN3v88internal12_GLOBAL__N_126GenericBinaryOpStub_StateXXXXX"
    const char *prefix = "_ZN3v88internal12_GLOBAL__N_126GenericBinaryOpStub_State";
    char name_buf[128];
    strcpy(name_buf, prefix);
    char *suffix_ptr = name_buf + strlen(prefix);

    struct arena *symbol_store = NULL;
    struct arena *name_store = NULL;

    for (int i = 0; i < NUM_SYMBOLS / NUM_THREADS; ++i) {
        sprintf(suffix_ptr, "%010d", (thread_id * NUM_SYMBOLS) + i);
        size_t len = strlen(name_buf) + 1;

        char *name = arena_alloc_dynamic(data->arenas, &name_store, len, 1);
        assert(name != NULL);
        memcpy(name, name_buf, len);

        struct symbol *sym = arena_alloc_dynamic(data->arenas, &symbol_store, sizeof(struct symbol), 64); 
        assert(sym != NULL);
        sym->name = name;
        sym->value = thread_id;

        uint64_t hash = hashfn(name_buf, len);
        struct htable_node *node = htable_insert(data->symbol_table, hash, name, len, &sym->hnode);
        assert(node == &sym->hnode);
    }

    return NULL;
}


void test_stress2()
{
    struct arena_list arenas = {0};
    struct htable symbols;

    htable_init(&symbols, NUM_SYMBOLS * MULTIPLIER);

    pthread_t threads[NUM_THREADS];
    struct thread_data data[NUM_THREADS];

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    for (int i = 0; i < NUM_THREADS; ++i) {
        data[i].arenas = &arenas;
        data[i].symbol_table = &symbols;
        data[i].thread_id = i + 1;
        data[i].count = 0;
        data[i].already = 0;
        data[i].lost_races = 0;

        pthread_create(&threads[i], NULL, stress_worker, &data[i]);
    }

    for (int i = 0; i < NUM_THREADS; ++i) {
        pthread_join(threads[i], NULL);
    }

    clock_gettime(CLOCK_MONOTONIC, &end);

    print_time(&start, &end);

    htable_diagnose(&symbols);

    arena_diagnose(&arenas);

    htable_free(&symbols);
    
    arena_list_free(&arenas);
}


void * worker(void *arg)
{
    struct thread_data *data = (struct thread_data*) arg;
    struct arena *symbols = NULL;

    int thread_id = data->thread_id;
    int seed = data->thread_id;

    for (int i = 0; i < NUM_SYMBOLS; ++i) {
        size_t offset = rand_r(&seed) % NUM_SYMBOLS;
        const char *name = &strings[offsets[offset]];
        size_t length = strlen(name) + 1;

        uint64_t hash = hashfn(name, length);

        if (htable_lookup(data->symbol_table, hash, name, length) != NULL) {
            data->already++;
            continue;
        }

        struct symbol *sym = arena_alloc_dynamic(data->arenas, &symbols, sizeof(struct symbol), sizeof(struct symbol));
        assert(sym != NULL);
        sym->name = name;
        sym->value = thread_id;

        struct htable_node *node = htable_insert(data->symbol_table, hash, sym->name, length, &sym->hnode);
        struct symbol *inserted = htable_entry(node, struct symbol, hnode);
        if (inserted == sym) {
            data->count++;
            assert(sym->value == thread_id);
        } else {
            data->lost_races++;
        }
    }

    return NULL;
}



void test_stress()
{
    struct arena_list arenas = {0};
    struct htable symbol_table;

    htable_init(&symbol_table, (NUM_SYMBOLS + 5) * MULTIPLIER);

    pthread_t threads[NUM_THREADS];
    struct thread_data data[NUM_THREADS];

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    for (int i = 0; i < NUM_THREADS; ++i) {
        data[i].arenas = &arenas;
        data[i].symbol_table = &symbol_table;
        data[i].thread_id = i + 1;
        data[i].count = 0;
        data[i].already = 0;
        data[i].lost_races = 0;

        pthread_create(&threads[i], NULL, worker, &data[i]);
    }

    for (int i = 0; i < NUM_THREADS; ++i) {
        pthread_join(threads[i], NULL);
    }

    clock_gettime(CLOCK_MONOTONIC, &end);

    print_time(&start, &end);

    size_t unique = 0;
    for (int i = 0; i < NUM_THREADS; ++i) {
        fprintf(stderr, "Thread %d: unique symbols=%d, lost races=%d, already=%d\n",
                data[i].thread_id, data[i].count, data[i].lost_races, data[i].already);
        unique += data[i].count;
    }
    fprintf(stderr, "Hash table size: %zu\n", htable_size(&symbol_table));
    fprintf(stderr, "Total symbols: %zu\n", unique);
    assert(htable_size(&symbol_table) == unique);

    for (int i = 0; i < 5; ++i) {
        const char *name = &strings[offsets[i]];
        size_t len = strlen(name) + 1;
        uint64_t hash = hashfn(name, len);

        const struct htable_node *node = htable_lookup(&symbol_table, hash, name, len);
        if (node == NULL) {
            continue;
        }
        const struct symbol *sym = htable_entry(node, struct symbol, hnode);
        fprintf(stderr, "Symbol %s made by thread %d\n", sym->name, sym->value);
    }

    htable_diagnose(&symbol_table);

    htable_free(&symbol_table);
    
    arena_diagnose(&arenas);
    arena_list_free(&arenas);
}


int main()
{
    strings = malloc((NUM_SYMBOLS + 5) * 32);
    assert(strings != NULL);

    offsets = malloc(sizeof(size_t) * (NUM_SYMBOLS + 5));

    fprintf(stderr, "number of symbols: %zu\n", NUM_SYMBOLS);
    fprintf(stderr, "number of threads: %zu\n", NUM_THREADS);

    char *ptr = strings;
    offsets[0] = ptr - strings;
    ptr = stpcpy(ptr, "malloc");
    ptr++;
    offsets[1] = ptr - strings;
    ptr = stpcpy(ptr, "free");
    ptr++;
    offsets[2] = ptr - strings;
    ptr = stpcpy(ptr, "main");
    ptr++;
    offsets[3] = ptr - strings;
    ptr = stpcpy(ptr, "printf");
    ptr++;
    offsets[4] = ptr - strings;
    ptr = stpcpy(ptr, "init");
    ptr++;

    for (int i = 0; i < NUM_SYMBOLS; ++i) {
        char buf[32];
        snprintf(buf, sizeof(buf), "sym%d", i);
        buf[31] = '\0';

        offsets[5 + i] = ptr - strings;
        ptr = stpcpy(ptr, buf);
        ptr++;
    }

    test_stress();
    //test_stress2();

    free(strings);
    free(offsets);
    return 0;
}
