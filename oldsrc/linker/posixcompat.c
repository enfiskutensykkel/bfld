#include <stddef.h>     // for size_t
#ifdef _WIN32
#include <io.h>         // for off_t
#else
#include <sys/types.h>  // for off_t
#endif


/*
 * Duplicate string.
 */
extern char * strdup(const char *s);


/*
 * Count characters in string but not beyond maxlen.
 */
size_t strnlen(const char *s, size_t maxlen);


/*
 * Extend or truncate a file to a given length.
 */
extern int ftruncate(int fd, off_t length);


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


#if (defined(_POSIX_C_SOURCE) && _POSIX_C_SOURCE >= 200112L)

#define HAS_FTRUNCATE 1

#else

#if defined(_BSD_SOURCE) || (defined(_XOPEN_SOURCE) && _XOPEN_SOURCE >= 500)
#define HAS_FTRUNCATE 1
#endif

#endif


#if !defined(HAS_STRDUP) || !HAS_STRDUP
#include <string.h>
#include <stdlib.h>
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
#include <string.h>
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


#if !defined(HAS_FTRUNCATE) || !HAS_TRUNCATE

#ifdef _WIN32
#include <io.h>
int ftruncate(int fd, off_t length)
{
    return _chsize(fd, length); // FIXME: is it chsize (without underscore)?
}

#else

#include <unistd.h>
#include <fcntl.h>

#if defined(F_CHSIZE)
int ftruncate(int fd, off_t length) 
{
    return fcntl(fd, F_CHSIZE, length);
}

#elif defined(F_FREESP)

#include <sys/stat.h>
#include <errno.h>

int ftruncate(int fd, off_t length)
{
    struct flock fl;
    struct stat stat;

    if (fstat (fd, &stat) < 0) {
        return -1;
    }

    if (stat.st_size < length) {
        // We need to extend the file length
        if (lseek (fd, (length - 1), SEEK_SET) < 0) {
            return -1;
        }

        // Write a NUL-byte to the end
        if (write (fd, "", 1) != 1) {
            return -1;
        }

    } else {
        // We need to truncate the length
        // This relies on the undocumented F_FREESP argument to fcntl,
        // allowing the file to be truncated so that it ends at fl.l_start
        // Shamelessly stolen from the gold linker source code, ftruncate.c

        fl.l_whence = 0;
        fl.l_len = 0;
        fl.l_start = length;
        fl.l_type = F_WRLCK;	/* write lock on file space */

        if (fcntl (fd, F_FREESP, &fl) < 0) {
            return -1
        }
    }

    return 0;
}

#else

#include <errno.h>

int ftruncate(int fd, off_t length)
{
    (void) fd;
    (void) length;

    errno = EIO;
    return -1;
}

#endif

#endif
#endif
