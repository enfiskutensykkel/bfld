#include "mfile.h"
#include "logging.h"
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <assert.h>


int mfile_open_read(struct mfile **file, const char *pathname)
{
    *file = NULL;

    log_ctx_new(pathname);

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

    log_debug("Opened file '%s'", pathname);

    // Create file handle
    struct mfile *f = malloc(sizeof(struct mfile));
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

    *file = f;

    log_ctx_pop();
    return 0;
}


struct mfile * mfile_get(struct mfile *file)
{
    assert(file != NULL);
    assert(file->refcnt > 0);
    ++(file->refcnt);
    return file;
}


void mfile_put(struct mfile *file)
{
    assert(file != NULL);
    assert(file->refcnt > 0);

    if (--(file->refcnt) == 0) {

        log_ctx_new(file->name);

        if (file->fd >= 0) {
            // If memory came from a file, unmap and close file
            munmap((void*) file->data, file->size);
            close(file->fd);
        } 

        log_debug("File closed");

        free(file->name);
        free(file);

        log_ctx_pop();
    }
}
