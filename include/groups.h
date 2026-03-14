#ifndef BFLD_SECTION_GROUPS_H
#define BFLD_SECTION_GROUPS_H
#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "strpool.h"


/*
 * Convenience wrapper for string pool to keep track of section groups.
 */
struct groups
{
    struct strpool names;
};


static inline
const char * group_name(const struct groups *groups, uint64_t group_id)
{
    return strpool_at(&groups->names, group_id);
}



static inline
uint64_t groups_create_group(struct groups *groups, const char *name)
{
    return strpool_intern(&groups->names, name);
}


static inline
uint64_t groups_lookup_group(const struct groups *groups, const char *name)
{ 
    return strpool_lookup(&groups->names, name);
}


static inline
void groups_clear(struct groups *groups)
{
    strpool_clear(&groups->names);
}


#ifdef __cplusplus
}
#endif
#endif
