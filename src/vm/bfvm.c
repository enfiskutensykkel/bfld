#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>

extern uint8_t *cells;

extern int foo();


int entrypoint(void)
{
    int v = foo();
    return v;
}
