#ifndef BFLD_UTILS_C_DEFINES_H
#define BFLD_UTILS_C_DEFINES_H


#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 2021112L && defined(__GNUC__)

#define __same_type(a, b) __builtin_types_compatible_p(typeof(a), typeof(b))


/*
 * Cast member of a structure out to the containing structure.
 * This is unsafe, use below instead.
 */
#define __containerof(ptr, type, member) ({ \
        void *__mptr = (void *)(ptr); \
        _Static_assert(__same_type(*(ptr), ((type *)0)->member) || \
                __same_type(*(ptr), void), \
                "pointer type mismatch in container_of()");	\
                ((type *)(__mptr - offsetof(type, member))); })


/*
 * Cast member of a structure out to the containing structure.
 * This preserves const-correctness.
 */
#define containerof(ptr, type, member) \
    _Generic(ptr, \
            const typeof(*(ptr)) *: ((const type *)__containerof(ptr, type, member)), \
            default: ((type *)__containerof(ptr, type, member)) \
            )


#else

#define containerof(ptr, type, member)  \
    ((type*) ((char*) ((void*) ptr) - offsetof(type, member)))

#endif
