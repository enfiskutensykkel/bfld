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


struct thread_data
{
    struct arena_list *arenas;
    struct htable *string_table;
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
};


uint64_t hashfn(const char *name, size_t length)
{
    return hash_xxh_32(name, length);
}


static char *strings = NULL;

static size_t *offsets = NULL;


void print_time(struct timespec *start, struct timespec *end)
{
    double elapsed = (end->tv_sec - start->tv_sec) * 1000 + (end->tv_nsec - start->tv_nsec) / 1e6;
    fprintf(stderr, "Test time: %.3f milliseconds\n", elapsed);
}


const char * intern_string(struct htable *string_table, const char *string, size_t length)
{
    uint64_t hash = hashfn(string, length);

    const char *interned = htable_get(string_table, hash, string, length);
    if (interned != NULL) {
        return interned;
    }

    return htable_put(string_table, hash, string, length, (void*) string);
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

        if (htable_get(data->symbol_table, hash, name, length) != NULL) {
            data->already++;
            continue;
        }

        struct symbol *s = arena_alloc_dynamic(data->arenas, &symbols, sizeof(struct symbol), sizeof(struct symbol));
        s->name = intern_string(data->string_table, name, length);
        s->value = thread_id;

        struct symbol *e = htable_put(data->symbol_table, hash, s->name, strlen(s->name) + 1, s);
        if (s == e) {
            data->count++;
            assert(s->value == thread_id);
        } else {
            data->lost_races++;
        }
    }

    return NULL;
}

void htable_diagnose(const struct htable *ht) 
{
    size_t total_probes = 0;
    size_t max_probe = 0;
    size_t filled_slots = 0;

    for (size_t i = 0; i < ht->capacity; i++) {
        uint64_t hash = atomic_load_explicit(&ht->slots[i].hash, memory_order_relaxed);
        if (hash != 0) {
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
        }
    }

    double load_factor = (double)filled_slots / ht->capacity;
    double avg_probe = filled_slots > 0 ? (double)total_probes / filled_slots : 0;

    fprintf(stderr, "\n--- Hash Table Diagnosis ---\n");
    fprintf(stderr, "Capacity   : %zu\n", ht->capacity);
    fprintf(stderr, "Filled     : %zu\n", filled_slots);
    fprintf(stderr, "Load Factor: %.2f%%\n", load_factor * 100.0);
    fprintf(stderr, "Avg Probe  : %.2f hops\n", avg_probe);
    fprintf(stderr, "Max Probe  : %zu hops\n", max_probe);
    fprintf(stderr, "----------------------------\n");
}


void test_stress()
{
    struct arena_list arenas = {0};
    struct htable string_table;
    struct htable symbol_table;

    htable_init(&string_table, (NUM_SYMBOLS + 5) * 2);
    htable_init(&symbol_table, (NUM_SYMBOLS + 5) * 2);

    pthread_t threads[NUM_THREADS];
    struct thread_data data[NUM_THREADS];

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    for (int i = 0; i < NUM_THREADS; ++i) {
        data[i].arenas = &arenas;
        data[i].string_table = &string_table;
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
    fprintf(stderr, "Total symbols: %zu\n", unique);
    assert(htable_size(&symbols) == unique);

    for (int i = 0; i < 5; ++i) {
        const char *name = &strings[offsets[i]];
        size_t len = strlen(name) + 1;
        uint64_t hash = hashfn(name, len);

        const struct symbol *s = htable_get(&symbol_table, hash, name, len);
        assert(s != NULL);
        fprintf(stderr, "Symbol %s made by thread %d\n", s->name, s->value);
        assert(htable_get(&string_table, hash, name, len) == s->name);
    }

    struct arena *curr = arenas.head;
    size_t total_size = 0;
    size_t total_used = 0;
    size_t count = 0;
    while (curr != NULL) {
        count++;
        total_size += curr->size;
        total_used += curr->used;
        curr = curr->next;
    }

    if (count < 10) {
        curr = arenas.head;
        size_t n = 0;
        while (curr != NULL) {
            fprintf(stderr, "Arena %zu: size=%zu, used=%zu, utilization=%.2f%%\n",
                    n++, curr->size, curr->used, ((double) curr->used / (double) curr->size) * 100.0);
            curr = curr->next;
        }
    }

    fprintf(stderr, "Total num arenas: %zu\n", count);
    fprintf(stderr, "Total allocated: %zu\n", total_size);
    fprintf(stderr, "Total used: %zu\n", total_used);
    fprintf(stderr, "Memory utilization: %.3f%%\n", ((double) total_used / (double) total_size) * 100.0);

    htable_diagnose(&string_table);
    htable_diagnose(&symbol_table);

    htable_free(&string_table);
    htable_free(&symbol_table);
    
    arena_list_free(&arenas);
}


int main()
{
    strings = malloc((NUM_SYMBOLS + 5) * 32);
    assert(strings != NULL);

    offsets = malloc(sizeof(size_t) * (NUM_SYMBOLS + 5));

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
    return 0;
}
