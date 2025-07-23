#ifndef __BFLD_UTILS_CONTAINER_OF_H__
#define __BFLD_UTILS_CONTAINER_OF_H__

/*
 * cast a member of a structure out to the containing structure
 */
#ifndef container_of
#define container_of(ptr, type, member) ({ \
    const typeof(((type*) 0)->member) *__mptr = (ptr); \
    (type*)((char*)__mptr - offsetof(type,member)); \
    })
#endif

#endif
