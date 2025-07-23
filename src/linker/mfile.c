#include "mfile.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>


int mfile_init(mfile **fhandle, const char *pathname)
{
    *fhandle = NULL;

    int fd = open(pathname, O_RDONLY);
    if (fd == -1) {
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
    if (fstat(fd, &s) == -1) {
        int status = errno;
        close(fd);
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

    // Memory-map the file
    void *p = mmap(NULL, s.st_size, PROT_READ, MAP_SHARED | MAP_POPULATE, fd, 0);
    if (p == MAP_FAILED) {
        int status = errno;
        close(fd);
        fprintf(stderr, "Could not read file %s: %s\n",
                pathname, strerror(status));
        return EBADF;
    }

    // Create file handle
    mfile *f = malloc(sizeof(mfile) + strlen(pathname) + 1);
    if (f == NULL) {
        munmap(p, s.st_size);
        close(fd);
        fprintf(stderr, "Failed to allocate handle\n");
        return ENOMEM;
    }

    f->refcnt = 1;
    f->fd = fd;
    f->size = s.st_size;
    f->data = p;
    strcpy(f->name, pathname);

    *fhandle = f;

    return 0;
}


void mfile_get(mfile *file)
{
    ++(file->refcnt);
}


void mfile_put(mfile *file)
{
    if (--(file->refcnt) == 0) {
        munmap((void*) file->data, file->size);
        close(file->fd);
        free(file);
    }
}
