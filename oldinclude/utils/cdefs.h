#ifndef BFLD_UTILS_C_DEFINES_H
#define BFLD_UTILS_C_DEFINES_H


#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L && (defined(__GNUC__) || defined(__clang__))

#undef offsetof
#define offsetof(type, member) __builtin_offsetof(type, member)

#define __same_type(a, b) __builtin_types_compatible_p(__typeof__(a), __typeof__(b))


/*
 * Cast member of a structure out to the containing structure.
 * This is unsafe, use below instead.
 */
#define __containerof(ptr, type, member) __extension__ ({ \
        void *__mptr = (void *)(ptr); \
        _Static_assert(__same_type(*(ptr), ((type *)0)->member) || \
                __same_type(*(ptr), void), \
                "pointer type mismatch in containerof()");	\
                ((type *)((char*) __mptr - offsetof(type, member))); })

/*
 * Cast member of a structure out to the containing structure.
 * This preserves const-correctness.
 */
#define containerof(ptr, type, member) \
    _Generic(ptr, \
            const __typeof__(*(ptr)) *: ((const type *) __containerof(ptr, type, member)), \
            default: ((type *) __containerof(ptr, type, member)) \
            )


/*
 * Expect the condition to be true
 */
#define likely(x)   __builtin_expect(!!(x), 1)

/*
 * Expect the condition to be false
 */
#define unlikely(x) __builtin_expect(!!(x), 0)


#else

#undef offsetof
#define offsetof(type, member) \
    ((size_t) &(((type*) 0)->member))


/*
 * Cast member of a structure out to the containing structure.
 * This is type unsafe and does not preserve const correctness.
 */
#define __containerof(ptr, type, member) \
    ((type*) ((char*) ((void*) ptr) - offsetof(type, member)))


#define containerof(ptr, type, member) __containerof(ptr, type, member)

#define likely(x)   !!(x)

#define unlikely(x) !!(x)

#endif

#endif
