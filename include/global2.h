#ifndef BFLD_GLOBALS_H
#define BFLD_GLOBALS_H
#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>


struct global
{
    uint32_t hash;
    uint32_t dfi;
    uint64_t name;
};


struct map_entry
{
    struct section *section;    // strong reference to section
    struct global *globals;     // dynamic array of globals that reference this section
    uint64_t capacity;          // capacity of the globals array
    uint64_t nglobals;          // number of globals in the array

};


struct globals
{

    uint64_t capacity;
};


#ifdef __cplusplus
}
#endif
#endif
