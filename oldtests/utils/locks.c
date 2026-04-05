#include "spinlock.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <time.h>
#include <stdint.h>
#include <stdlib.h>

struct thread_data
{
    pthread_barrier_t *barrier;
    volatile long *shared;
    int thread_id;
    int iterations;
    void *lock;
    void (*lockfn)(void*);
    void (*unlockfn)(void*);
};


int gauss(int n) 
{
    return (n * (n + 1)) / 2;
}


void * test_writer(void *arg)
{
    struct thread_data *data = arg;

    volatile long *shared = data->shared;

    pthread_barrier_wait(data->barrier);

    for (int i = 0, n = data->iterations; i < n; ++i) {
        data->lockfn(data->lock);
        for (volatile int j = 1; j <= 100; ++j) {
            *shared += j;
        }
        data->unlockfn(data->lock);
    }

    pthread_barrier_wait(data->barrier);

    return NULL;
}


void * test_reader(void *arg)
{
    struct thread_data *data = arg;

    pthread_barrier_wait(data->barrier);

    for (int i = 0, n = data->iterations; i < n; ++i) {
        data->lockfn(data->lock);
        long value = *(data->shared);
        for (volatile int j = 1; j <= 100; ++j) {
            value += j;
        }
        (void) value;
        data->unlockfn(data->lock);
    }

    pthread_barrier_wait(data->barrier);

    return NULL;
}


struct timespec get_time(void) 
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return now;
}


double elapsed_time(const struct timespec *start, const struct timespec *end)
{
    double elapsed = (end->tv_sec - start->tv_sec) + (end->tv_nsec - start->tv_nsec) / 1e9;
    return elapsed;
}


double mixed_bench(const void *name, void *lock_data,
                 void (*rdlock)(void*), void (*rdunlock)(void*),
                 void (*wrlock)(void*), void (*wrunlock)(void*))
{
    const int num_readers = 6;
    const int num_writers = 2;
    const int num_threads = num_readers + num_writers;
    long shared = 0;
    const int num_iterations = 10000000;

    struct thread_data td[num_threads];
    pthread_t threads[num_threads];

    pthread_barrier_t barrier;
    pthread_barrier_init(&barrier, NULL, num_threads + 1);

    for (int i = 0; i < num_readers; ++i) {
        td[i].barrier = &barrier;
        td[i].shared = &shared;
        td[i].iterations = num_iterations;
        td[i].thread_id = i;
        td[i].lock = lock_data;
        td[i].lockfn = rdlock;
        td[i].unlockfn = rdunlock;
        pthread_create(&threads[i], NULL, test_reader, &td[i]);
    }

    for (int i = 0; i < num_writers; ++i) {
        int t = i + num_readers;
        td[t].barrier = &barrier;
        td[t].shared = &shared;
        td[t].iterations = num_iterations;
        td[t].thread_id = t;
        td[t].lock = lock_data;
        td[t].lockfn = wrlock;
        td[t].unlockfn = wrunlock;
        pthread_create(&threads[t], NULL, test_writer, &td[t]);
    }
    
    pthread_barrier_wait(&barrier);
    struct timespec start = get_time();

    pthread_barrier_wait(&barrier);
    struct timespec end = get_time();

    for (int i = 0; i < num_threads; ++i) {
        pthread_join(threads[i], NULL);
    }

    double elapsed = elapsed_time(&start, &end);
    fprintf(stderr, "%-20s | r=%-2d w=%-2d N=%-2d | %-20s | time=%.5fs\n", 
            __func__,
            num_readers,
            num_writers,
            num_threads,
            name,
            elapsed);

    pthread_barrier_destroy(&barrier);
}


double writer_bench(const void *name, void *lock_data, void (*lock)(void*), void (*unlock)(void*)) {
    const long num_threads = 8;
    const long iterations = 500000;
    long shared = 0;
    struct thread_data td[num_threads];
    pthread_t threads[num_threads];

    pthread_barrier_t barrier;
    pthread_barrier_init(&barrier, NULL, num_threads + 1);

    for (int i = 0; i < num_threads; ++i) {
        td[i].barrier = &barrier;
        td[i].shared = &shared;
        td[i].thread_id = i;
        td[i].iterations = iterations;
        td[i].lock = lock_data;
        td[i].lockfn = lock;
        td[i].unlockfn = unlock;

        pthread_create(&threads[i], NULL, test_writer, &td[i]);
    }

    pthread_barrier_wait(&barrier);
    struct timespec start = get_time();

    pthread_barrier_wait(&barrier);
    struct timespec end = get_time();

    for (int i = 0; i < num_threads; ++i) {
        pthread_join(threads[i], NULL);
    }

    double elapsed = elapsed_time(&start, &end);

    long expected = gauss(100);
    expected *= num_threads;
    expected *= iterations;
    assert(shared == expected);
    fprintf(stderr, "%-20s | r=%-2d w=%-2d N=%-2d | %-20s | time=%.5fs\n", 
            __func__,
            0, num_threads, num_threads,
            name,
            elapsed);

    pthread_barrier_destroy(&barrier);
    return elapsed;
}


int main(void)
{

    pthread_mutex_t mutex;
    pthread_mutex_init(&mutex, NULL);
    double mutex_time = writer_bench("pthread_mutex", &mutex, 
            (void (*)(void*)) pthread_mutex_lock, 
            (void (*)(void*)) pthread_mutex_unlock);
    pthread_mutex_destroy(&mutex);

    struct spinlock spinlock = {0};
    double spinlock_time = writer_bench("spinlock", &spinlock, 
            (void (*)(void*)) spinlock_lock, 
            (void (*)(void*)) spinlock_unlock);

    pthread_rwlock_t prwlock_wb, prwlock_rb;
    pthread_rwlockattr_t wbattr, rbattr;

    pthread_rwlockattr_setkind_np(&wbattr, PTHREAD_RWLOCK_PREFER_WRITER_NP);
    pthread_rwlockattr_setkind_np(&rbattr, PTHREAD_RWLOCK_PREFER_READER_NP);

    pthread_rwlockattr_init(&wbattr);
    pthread_rwlockattr_init(&rbattr);

    pthread_rwlock_init(&prwlock_wb, NULL);
    pthread_rwlock_init(&prwlock_rb, NULL);

    double prwlock_wb_time = mixed_bench("pthread_rwlock (wb)", &prwlock_wb,
            (void (*)(void*)) pthread_rwlock_rdlock,
            (void (*)(void*)) pthread_rwlock_unlock,
            (void (*)(void*)) pthread_rwlock_wrlock,
            (void (*)(void*)) pthread_rwlock_unlock);

    double prwlock_rb_time = mixed_bench("pthread_rwlock (rb)", &prwlock_rb,
            (void (*)(void*)) pthread_rwlock_rdlock,
            (void (*)(void*)) pthread_rwlock_unlock,
            (void (*)(void*)) pthread_rwlock_wrlock,
            (void (*)(void*)) pthread_rwlock_unlock);

    struct rwlock rwlock = {0};

    double rwlock_wb_time = mixed_bench("rwlock (wb)", &rwlock,
            (void (*)(void*)) rwlock_read_lock,
            (void (*)(void*)) rwlock_read_unlock,
            (void (*)(void*)) rwlock_biased_write_lock,
            (void (*)(void*)) rwlock_write_unlock);

    double rwlock_rb_time = mixed_bench("rwlock (rb)", &rwlock,
            (void (*)(void*)) rwlock_biased_read_lock,
            (void (*)(void*)) rwlock_read_unlock,
            (void (*)(void*)) rwlock_write_lock,
            (void (*)(void*)) rwlock_write_unlock);

    double spinlock_rw_time = mixed_bench("spinlock", &spinlock,
            (void (*)(void*)) spinlock_lock, 
            (void (*)(void*)) spinlock_unlock,
            (void (*)(void*)) spinlock_lock, 
            (void (*)(void*)) spinlock_unlock);

    pthread_rwlock_destroy(&prwlock_rb);
    pthread_rwlock_destroy(&prwlock_wb);

    pthread_rwlockattr_destroy(&wbattr);
    pthread_rwlockattr_destroy(&rbattr);

    if (mutex_time < spinlock_time) {
        exit(1);
    }

    exit(0);
}
