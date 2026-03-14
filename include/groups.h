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
 * Helper structure to keep track of section groups such as COMDAT.
 */ 
struct groups
{
    struct strpool signatures;  // string pool group signatures (offset = group_id)
    uint64_t *comdat;           // keep track of which groups are COMDAT groups
                                // (bitmap where group_id is index)
};


/*
 * Get the group name.
 */
static inline
const char * group_name(const struct groups *groups, uint64_t group_id)
{
    return strpool_at(&groups->signatures, group_id);
}


/*
 * Check if group is a COMDAT group.
 */
static inline
bool groups_is_comdat_group(const struct groups *groups, uint64_t group_id)
{
    if (groups->comdat != NULL && group_id < groups->signatures.offset) {
        return !!(groups->comdat[group_id >> 6] & (1ULL << (group_id & 63)));
    }
    return false;
}


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
