#ifndef BFLD_SECTION_GROUPS_H
#define BFLD_SECTION_GROUPS_H
#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "sections.h"

struct section;


/*
 * Section group representation.
 */
struct group
{
    uint32_t hash;
    size_t dfi;
    char *name;
    struct sections sections;
};


struct groups
{
    struct group *table;
    size_t capacity;
    size_t ngroups;
    size_t rehash_threshold;
};


static inline
const char * group_name(const struct group *group)
{
    return group->name;
}


static inline
bool group_add_section(struct group *group, struct section *section)
{
    if (section->group != NULL) {
        return false;
    }

    return sections_push(&group->sections, section);
}


static inline bool group_empty(const struct group *group)
{
    return sections_empty(&group->sections);
}


struct group * groups_create(struct groups *groups, const char *name, size_t nsections);


struct group * groups_lookup(const struct groups *groups, const char *name);


void groups_remove(struct groups *groups, const char *name);


void groups_clear(struct groups *groups);


#ifdef __cplusplus
}
#endif
#endif
