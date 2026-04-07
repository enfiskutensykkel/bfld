#ifndef BFLD_OBJECT_FILE_H
#define BFLD_OBJECT_FILE_H
#ifdef __cplusplus
extern "C" {
#endif

#include "cdefs.h"
#include <stddef.h>
#include <stdint.h>

struct mfile;

/*
 * Object file descriptor.
 */
struct objectfile
{
    const char *name;
    const uint8_t *data;
    uint32_t march;
    size_t size;
    size_t nsections;
    size_t nsymbols;
};




#ifdef __cplusplus
}
#endif
#endif
