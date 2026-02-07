#ifndef BFLD_GROUP_H
#define BFLD_GROUP_H
#ifdef __cplusplus
extern "C" {
#endif

struct section;


/*
 * Section group representation.
 */
struct group
{
    char *name;
    int refcnt;
    struct section *sections;
    size_t nsections;
};


struct group * group_alloc(size_t nsections);


struct group * group_get(struct group *group);


void group_put(struct group *group);


#ifdef __cplusplus
}
#endif
#endif
