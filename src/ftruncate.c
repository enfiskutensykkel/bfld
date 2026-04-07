#include "cdefs.h"
#include <stddef.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>


#if (defined(_POSIX_C_SOURCE) && _POSIX_C_SOURCE >= 200112L) \
    || defined(_BSD_SOURCE) || (defined(_XOPEN_SOURCE) && _XOPEN_SOURCE >= 500)
/* We have ftruncate() */

#elif defined(F_CHSIZE)
int ftruncate(int fd, off_t length)
{
    return fcntl(fd, F_CHSIZE, length);
}

#elif defined(F_FREESP)
int ftruncate(int fd, off_t length)
{
    struct flock fl;
    struct stat stat;

    if (fstat(fd, &stat) < 0) {
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

        if (fcntl(fd, F_FREESP, &fl) < 0) {
            return -1
        }
    }

    return 0;
}

#else

int ftruncate(int fd, off_t length)
{
    (void) fd;
    (void) length;

    errno = EIO;
    return -1;
}

#endif
