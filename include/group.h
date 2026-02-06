#ifndef BFLD_GROUP_H
#define BFLD_GROUP_H
#ifdef __cplusplus
extern "C" {
#endif


#include "sections.h"


/*
 * Section group representation.
 */
struct group
{
    char *name;
    int refcnt;
    struct sections sections;
};


#ifdef __cplusplus
}
#endif
#endif
