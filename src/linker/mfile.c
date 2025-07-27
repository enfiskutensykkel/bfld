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

    log_ctx_push(LOG_CTX_FILE(NULL, pathname));

    int fd = open(pathname, O_RDONLY);
    if (fd == -1) {
        log_error(strerror(errno));
        log_ctx_pop();
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
        log_error(strerror(status));
        log_ctx_pop();
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
        log_error("Unable to memory-map file; %s", strerror(status));
        log_ctx_pop();
        return EBADF;
    }

    // Create file handle
    mfile *f = malloc(sizeof(mfile));
    if (f == NULL) {
        munmap(p, s.st_size);
        close(fd);
        log_ctx_pop();
        return ENOMEM;
    }

    f->name = strdup(pathname);
    if (f->name == NULL) {
        free(f);
        munmap(p, s.st_size);
        close(fd);
        log_ctx_pop();
        return ENOMEM;
    }

    f->refcnt = 1;
    f->fd = fd;
    f->size = s.st_size;
    f->data = p;
    strcpy(f->name, pathname);

    *fhandle = f;

    log_ctx_pop();
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
            free(file->name);
            free(file);
        }
    }
}
