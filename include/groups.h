#ifndef BFLD_SECTION_GROUPS_H
#define BFLD_SECTION_GROUPS_H
#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include "strpool.h"


/*
 * Helper structure to keep track of section groups;
 */
struct groups
{
    struct strpool signatures;  // internal string pool
};


/*
 * Try to find the section group with the given signature.
 */
static inline
uint64_t groups_lookup_group(const struct groups *groups, const char *signature)
{
    return strpool_lookup(&groups->signatures, signature);
}


/*
 * Register section group.
 */
uint64_t groups_create_group(struct groups *groups, 
                             const char *signature, 
                             bool comdat);


/*
 * Clean up groups.
 */
void groups_clear(struct groups *groups);


#define groups_for_each_group(iterator, group_ptr) \
    strpool_for_each_offset(iterator, &(group_ptr)->signatures)


#ifdef __cplusplus
}
#endif
#endif
