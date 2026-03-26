#include <deque.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <assert.h>


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
    fprintf(stderr, "head=%zu size=%zu capacity=%zu\n", d.head, deque_size(&d), d.capacity);

    for (uintptr_t i = 3; i <= 11; ++i) {
        uintptr_t v = (uintptr_t) deque_pop_front(&d);
        assert(v == i);
    }

    fprintf(stderr, "test passed\n");
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
    return 0;
}
