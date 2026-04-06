#include <tbb/concurrent_hash_map.h>
#include <memory>
#include <ctime>
#include <cstdio>
#include <pthread.h>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include "hash.h"
#include "arena.h"


#define NUM_THREADS 8
#define NUM_SYMBOLS 1000000
#define MULTIPLIER 2

struct symbol
{
    const char *name;
    int value;
    //struct htable_node hnode;
};


struct cstr
{
    static size_t hash(const char *s)
    {
        size_t length = strlen(s) + 1;
        return hash_xxh_32(s, length);
    }

    static bool equal(const char *a, const char *b)
    {
        return strcmp(a, b) == 0;
    }
};


//struct cstrhash
//{
//    size_t operator()(const char *s) const
//    {
//        size_t length = strlen(s) + 1;
//        return hash_xxh_32(s, length);
//    }
//};
//
//
//struct cstrequal
//{
//    bool operator()(const char *a, const char *b) const
//    {
//        size_t an = strlen(a) + 1;
//        size_t bn = strlen(b) + 1;
//        if (an != bn) {
//            return false;
//        }
//        return memcmp(a, b, an) == 0;
//    }
//};

//using SymbolTable = std::unordered_map<const char*,std::unique_ptr<symbol>, cstrhash, cstrequal>;
using SymbolTable = tbb::concurrent_hash_map<const char*, symbol*, cstr>;

struct thread_data
{
    struct arena_list *arenas;
    SymbolTable *symbol_table;
    int thread_id;
    int count;
    int lost_races;
    int already;
};


static char *strings = NULL;

static size_t *offsets = NULL;


void * worker(void *arg)
{
    struct thread_data *data = (struct thread_data*) arg;

    auto& symbol_table = *data->symbol_table;
    struct arena *symbols = NULL;

    int thread_id = data->thread_id;
    unsigned int seed = data->thread_id;

    for (int i = 0; i < NUM_SYMBOLS; ++i) {
        size_t offset = rand_r(&seed) % NUM_SYMBOLS;
        const char *name = &strings[offsets[offset]];
        size_t length = strlen(name) + 1;

        /*
        auto sym = std::make_unique<symbol>(name, thread_id);
        auto [it, inserted] = symbol_table.try_emplace(sym->name, std::move(sym));

        if (inserted) {
            data->count++;
        } else {
            data->lost_races++;
        }*/

        SymbolTable::accessor acc;
        if (symbol_table.insert(acc, name)) {
            symbol *sym = (symbol*) arena_alloc_dynamic(data->arenas, &symbols, sizeof(symbol), sizeof(symbol));
            sym->name = name;
            sym->value = thread_id;
            acc->second = sym;
            data->count++;
        } else {
            data->lost_races++;
        }
    }

    return NULL;
}

void print_time(const struct timespec& start, const struct timespec& end)
{
    double elapsed = (end.tv_sec - start.tv_sec) * 1000 + (end.tv_nsec - start.tv_nsec) / 1e6;
    fprintf(stderr, "Test time: %.3f milliseconds\n", elapsed);
}


void test_stress()
{
    struct arena_list arenas;
    arena_list_init(&arenas);

    SymbolTable symbol_table;
    pthread_t threads[NUM_THREADS];
    struct thread_data data[NUM_THREADS];

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    //symbol_table.reserve(NUM_SYMBOLS + 5);
    symbol_table.rehash(NUM_SYMBOLS + 5 * MULTIPLIER);

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

    print_time(start, end);

    size_t unique = 0;
    for (int i = 0; i < NUM_THREADS; ++i) {
        fprintf(stderr, "Thread %d: unique symbols=%d, lost races=%d, already=%d\n",
                data[i].thread_id, data[i].count, data[i].lost_races, data[i].already);
        unique += data[i].count;
    }
    fprintf(stderr, "Hash table size: %zu\n", symbol_table.size());
    fprintf(stderr, "Total symbols: %zu\n", unique);

    for (int i = 0; i < 5; ++i) {
        const char *name = &strings[offsets[i]];

        SymbolTable::const_accessor acc;

        if (symbol_table.find(acc, name)) {
            const auto& sym = acc->second;
            fprintf(stderr, "Symbol %s made by thread %d\n", sym->name, sym->value);
        }
    }
}


int main()
{
    strings = (char*) malloc((NUM_SYMBOLS + 5) * 32);
    assert(strings != NULL);

    offsets = (size_t*) malloc(sizeof(size_t) * (NUM_SYMBOLS + 5));

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

    free(strings);
    free(offsets);

    //auto start = std::chrono::high_resolution_clock::now();
    //auto stop = std::chrono::high_resolution_clock::now();
    //auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);
    return 0;
}
