#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>


extern uint8_t *cells;
extern const uint8_t *ip;

extern int foo(void);


int interpreter_entry(void)
{
    uint8_t *ptr = cells;
    foo();
    return *ptr;
}
