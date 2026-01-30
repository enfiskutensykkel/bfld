#include <stddef.h>
#include <stdlib.h>
#include <string.h>


#if (defined(_POSIX_C_SOURCE) && _POSIX_C_SOURCE >= 200809L)

#define HAS_STRLEN 1
#define HAS_STRDUP 1

#else

#if defined(_GNU_SOURCE)
#define HAS_STRNLEN 1
#endif

#if (defined(_BSD_SOURCE) || defined(_SVID_SOURCE) || (defined(_XOPEN_SOURCE) && _XOPEN_SOURCE >= 500))
#define HAS_STRDUP 1
#endif

#endif


#if !defined(HAS_STRDUP) || !HAS_STRDUP
char * strdup(const char *s)
{
    size_t n = strlen(s);

    char *p = malloc(n + 1);
    if (p == NULL) {
        return NULL;
    }
    strcpy(p, s);
    return p;
}
#endif


#if !defined(HAS_STRNLEN) || !HAS_STRNLEN
size_t strnlen(const char *s, size_t maxlen)
{
    size_t n = 0;

    while (maxlen-- > 0) {
        if (*s++ == '\0') {
            return n;
        }
        ++n;
    }

    return n;
}
#endif
