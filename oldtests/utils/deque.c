#include <deque.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>

#define NUM_PRODUCERS 4
#define NUM_CONSUMERS 6


struct thread_data
{
    uint64_t thread_id;
    uint64_t num_items;
    uint64_t num_producers;
    struct deque *deque;
    uint64_t produced;
    uint64_t consumed;
    _Atomic uint64_t *total_consumed;
};


void * producer(void *arg)
{
    struct thread_data *data = arg;
    uint64_t thread_id = data->thread_id;
    uint64_t num_items = data->num_items;
    uint64_t produced = 0;
    struct deque *deque = data->deque;

    uint64_t seed = thread_id;

    for (uint64_t i = 1; i <= num_items; ++i) {
        uintptr_t value = i + (thread_id * num_items * 10UL);
        bool x = false;

        if (thread_id % 2 == 0) {
            x = deque_push_back(deque, (void*) value);
        } else {
            x = deque_push_front(deque, (void*) value);
        }

        assert(x == true);

        produced++;

        //if (i % 10 == 0) {
        //    sched_yield();
        //}
    }

    data->produced = produced;
    assert(data->produced == num_items);

    return NULL;
}


void * consumer(void *arg)
{
    struct thread_data *data = arg;
    uint64_t thread_id = data->thread_id;
    struct deque *deque = data->deque;

    uint64_t consumed = 0;
    _Atomic uint64_t *total_consumed = data->total_consumed;
    uint64_t expected_total = data->num_items * data->num_producers;

    while (atomic_load_explicit(total_consumed, memory_order_relaxed) < expected_total) {
        
        void *item = NULL;

        if (thread_id % 2 == 0) {
            item = deque_pop_front(deque);
        } else {
            item = deque_pop_back(deque);
        }

        if (item != NULL) {
            atomic_fetch_add_explicit(total_consumed, 1, memory_order_relaxed);
            consumed++;
        } else {
            sched_yield();
        }
    }

    data->consumed = consumed;

    return NULL;
}


void test_multithreaded()
{
    pthread_t threads[NUM_PRODUCERS + NUM_CONSUMERS];

    struct thread_data data[NUM_PRODUCERS + NUM_CONSUMERS];
    const uint64_t num_items = 10000000UL;

    _Atomic uint64_t total_consumed;
    atomic_init(&total_consumed, 0);

    struct deque deque = {0};
    deque_reserve(&deque, 1024);

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    for (int i = 0; i < NUM_PRODUCERS + NUM_CONSUMERS; ++i) {
        data[i].thread_id = i;
        data[i].num_items = num_items;
        data[i].num_producers = NUM_PRODUCERS;
        data[i].deque = &deque;
        data[i].produced = 0;
        data[i].consumed = 0;
        data[i].total_consumed = &total_consumed;
    }

    for (int i = 0; i < NUM_PRODUCERS; ++i) {
        pthread_create(&threads[i], NULL, producer, &data[i]);
    }
    for (int i = 0; i < NUM_CONSUMERS; ++i) {
        pthread_create(&threads[NUM_PRODUCERS + i], NULL, consumer, &data[NUM_PRODUCERS + i]);
    }

    for (int i = 0; i < NUM_PRODUCERS + NUM_CONSUMERS; ++i) {
        pthread_join(threads[i], NULL);
    }
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    double elapsed = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
    fprintf(stderr, "Time: %.4f seconds\n", elapsed);
    fprintf(stderr, "Total consumed %ld / %ld\n",
            atomic_load(&total_consumed), NUM_PRODUCERS * num_items);

    for (int i = 0; i < NUM_PRODUCERS + NUM_CONSUMERS; ++i) {
        fprintf(stderr, "[thread %2lu] - %10lu produced / %10lu consumed\n",
                data[i].thread_id, data[i].produced, data[i].consumed);
    }

    assert(atomic_load(&total_consumed) == (NUM_PRODUCERS * num_items));
    assert(deque_empty(&deque));

    deque_clear(&deque);
}



void test_resize_wrap()
{
    uintptr_t x;
    struct deque d = {0};

    deque_reserve(&d, 8);
    assert(d.capacity == 8);

    for (uintptr_t i = 1; i <= 6; ++i) {
        deque_push_back(&d, (void*) i);
    }
    assert(d.capacity == 8);

    x = (uintptr_t) deque_pop_front(&d);
    assert(x == 1);
    x = (uintptr_t) deque_pop_front(&d);
    assert(x == 2);

    // d->q should now be [NULL, NULL, 3, 4, 5, 6, NULL, NULL]
    assert(d.head == 2 && deque_size(&d) == 4);
    assert((uintptr_t) deque_front(&d) == 3);
    assert((uintptr_t) deque_back(&d) == 6);

    deque_push_back(&d, (void*) 7);
    deque_push_back(&d, (void*) 8);
    deque_push_back(&d, (void*) 9);
    deque_push_back(&d, (void*) 10);

    // d->q should now be [9, 10, 3, 4, 5, 6, 7, 8]
    assert(d.head == 2);
    assert(deque_size(&d) == 8);
    assert(d.capacity == 8);
    assert((uintptr_t) deque_front(&d) == 3);
    assert((uintptr_t) deque_back(&d) == 10);

    // trigger resize
    deque_push_back(&d, (void*) 11);
    //fprintf(stderr, "head=%zu size=%zu capacity=%zu\n", d.head, deque_size(&d), d.capacity);

    for (uintptr_t i = 3; i <= 11; ++i) {
        uintptr_t v = (uintptr_t) deque_pop_front(&d);
        assert(v == i);
    }

    deque_clear(&d);
}


void test_circular()
{
    struct deque dq = DEQUE_INIT;

    int values[] = {10, 20, 30, 40};

    deque_push_back(&dq, &values[0]);
    deque_push_back(&dq, &values[1]);
    assert(deque_size(&dq) == 2);

    int *v = deque_pop_front(&dq);
    assert(*v == 10);
    assert(deque_size(&dq) == 1);

    deque_push_back(&dq, &values[2]);

    deque_push_front(&dq, &values[3]);

    assert(deque_size(&dq) == 3);

    assert(*((int*) deque_pop_front(&dq)) == 40);
    assert(*((int*) deque_pop_front(&dq)) == 20);
    assert(*((int*) deque_pop_front(&dq)) == 30);

    deque_clear(&dq);
}


void test_realloc()
{
    struct deque d = DEQUE_INIT;

    for (int i = 0; i < 6; ++i) {
        deque_push_back(&d, (void*) (uintptr_t) i);
    }

    for (int i = 0; i < 4; ++i) {
        deque_pop_front(&d);
    }

    for (int i = 6; i <= 11; ++i) {
        deque_push_back(&d, (void*) (uintptr_t) i);
    }

    assert(d.size == 8);
    assert(d.capacity == 8);
    assert(d.head == 4);
    assert(d.head + d.size > d.capacity);

    deque_push_back(&d, (void*) (uintptr_t) 12);
    assert(d.size == 9);
    assert(d.capacity == 16);
    assert(d.head == 12); // 4 + 8

    uint64_t expected = 4;
    while (!deque_empty(&d)) {
        uint64_t value = (uint64_t) (uintptr_t) deque_pop_front(&d);
        assert(value == expected);
        expected++;
    }
    assert(expected == 13);

    deque_clear(&d);
}


int main(int argc, char **argv)
{
    test_resize_wrap();
    test_circular();
    test_realloc();
    test_multithreaded();
    return 0;
}
