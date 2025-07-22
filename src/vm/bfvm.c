#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>


extern uint8_t *cells;

static uint8_t some_array[16];

extern int extremely_long_name_to_see_how_long_the_name_can_be();
//extern int foo();

extern int foo();

int entrypoint(void)
{
    int v = foo();
    cells[0] = 0;
    some_array[0] = 0;
    return v;
}
