#include <deque.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <assert.h>

named_deque(int, intdeque);


int main(int argc, char **argv)
{
    struct intdeque dq;

    int values[] = {10, 20, 30, 40};

    intdeque_push_back(&dq, &values[0]);
    intdeque_push_back(&dq, &values[1]);
    assert(intdeque_size(&dq) == 2);

    int *v = intdeque_pop_front(&dq);
    assert(*v == 10);
    assert(intdeque_size(&dq) == 1);

    intdeque_push_back(&dq, &values[2]);

    intdeque_push_front(&dq, &values[3]);

    assert(intdeque_size(&dq) == 3);

    assert(*intdeque_pop_front(&dq) == 40);
    assert(*intdeque_pop_front(&dq) == 20);
    assert(*intdeque_pop_front(&dq) == 30);

    return 0;
}
