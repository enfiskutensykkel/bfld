#include "mfile.h"
#include "logging.h"
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
        log_fatal("Failed to open file: %s", strerror(errno));
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
        log_fatal("Could not get file information about file: %s", strerror(status));
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
        log_fatal("Unable to memory-map file: %s", strerror(status));
        return EBADF;
    }

    // Create file handle
    mfile *f = malloc(sizeof(mfile) + strlen(pathname) + 1);
    if (f == NULL) {
        munmap(p, s.st_size);
        close(fd);
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
    if (file != NULL) {
        ++(file->refcnt);
    }
}


void mfile_put(mfile *file)
{
    if (file != NULL) {
        if (--(file->refcnt) == 0) {
            munmap((void*) file->data, file->size);
            close(file->fd);
            free(file);
        }
    }
}
