#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>


extern uint8_t *cells;
extern const uint8_t *ip;


int interpreter_entry(void)
{
    uint8_t *ptr = cells;
    return *ptr;
}
