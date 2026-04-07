#ifndef BFLD_LINKER_H
#define BFLD_LINKER_H
#ifdef __cplusplus
extern "C" {
#endif

#include "cdefs.h"
#include <stddef.h>


struct mfile;


struct linkerctx
{
    struct mfile **files;
};

#ifdef __cplusplus
}
#endif
#endif
