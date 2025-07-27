#include <stdint.h>

int a_weak_symbol __attribute__((weak));

extern uint32_t *an_external_pointer;

static uint8_t a_local_array[16];

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
