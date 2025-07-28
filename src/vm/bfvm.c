#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>


int a_weak_symbol __attribute__((weak));

extern uint32_t *an_external_pointer;

static uint16_t a_local_array[16];

static void a_static_function(void)
{
    *an_external_pointer = 0xdeadbeef;
}

int extremely_long_name_to_see_how_long_the_name_can_be(void)
{
    return 42;
}

int foo(void)
{
    return 1;
}

int bar(void)
{
    return 2;
}

extern uint8_t *cells;


int entrypoint(void)
{
    int v = foo();
    cells[0] = 0;
    some_array[0] = 0;
    return v;
}

int bar(void)
{
    return 9001;
}
