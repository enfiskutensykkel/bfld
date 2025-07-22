#include "io.h"
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>


int ifile_open(struct ifile *fp, const char *pathname)
{
    struct ifile f;

    fp->fd = -1;
    fp->ptr = NULL;
    fp->size = 0;

    f.fd = open(pathname, O_RDONLY);
    if (f.fd == -1) {
        fprintf(stderr, "Could not open input file %s: %s\n",
                pathname, strerror(errno));
        switch (errno) {
            case EACCES:
            case EPERM:
            case ENOENT:
                return errno;

            default:
                return EBADF;
        }
    }

    // Get the file size so that the entire file can be memory-mapped
    struct stat s;
    if (fstat(f.fd, &s) == -1) {
        int status = errno;
        close(f.fd);
        fprintf(stderr, "Could not get file information about file %s: %s\n",
                pathname, strerror(status));
        switch (status) {
            case EACCES:
            case EPERM:
            case ENOENT:
                return status;

            default:
                return EBADF;
        }
    }
    f.size = s.st_size;

    // Memory-map the file
    f.ptr = mmap(NULL, f.size, PROT_READ, MAP_SHARED | MAP_POPULATE, f.fd, 0);
    if (f.ptr == MAP_FAILED) {
        int status = errno;
        close(f.fd);
        fprintf(stderr, "Could not read file %s: %s\n",
                pathname, strerror(status));
        return EBADF;
    }

    *fp = f;

    return 0;
}


void ifile_close(struct ifile *fp)
{
    if (fp->ptr != NULL && fp->size > 0) {
        munmap((void*) fp->ptr, fp->size);
        fp->ptr = NULL;
        fp->size = 0;
    }

    if (fp->fd > 0) {
        close(fp->fd);
        fp->fd = -1;
    }
}
