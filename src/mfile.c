#include "cdefs.h"
#include "mfile.h"
#include "logging.h"
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <assert.h>


extern int ftruncate(int fd, off_t length);


int mfile_open(struct mfile **file, const char *pathname)
{
    *file = NULL;

    log_ctx_new(pathname);

    int fd = open(pathname, O_RDONLY);
    if (fd == -1) {
        log_error("Unable to open file");
        log_ctx_pop();
        switch (errno) {
            case EACCES:
            case EPERM:
            case ENOENT:
            case EISDIR:
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
        log_error("Unable to check file size");
        log_ctx_pop();
        switch (status) {
            case EACCES:
            case EPERM:
            case ENOENT:
            case EISDIR:
                return status;

            default:
                return EBADF;
        }
    }

    // Memory-map the file
    int flags = MAP_SHARED | MAP_POPULATE;
    void *p = mmap(NULL, s.st_size, PROT_READ, flags, fd, 0);
    if (p == MAP_FAILED) {
        //int status = errno;
        close(fd);
        log_error("Unable to memory-map file");
        log_ctx_pop();
        return EBADF;
    }

#ifdef HAS_MADVISE
    madvise(p, st.st_size, MADV_RANDOM);
#endif

    log_trace("File opened");

    // Create file handle
    struct mfile *f = malloc(sizeof(struct mfile));
    if (f == NULL) {
        munmap(p, s.st_size);
        close(fd);
        log_ctx_pop();
        return ENOMEM;
    }

    f->name = malloc(strlen(pathname) + 1);
    if (f->name == NULL) {
        free(f);
        munmap(p, s.st_size);
        close(fd);
        log_ctx_pop();
        return ENOMEM;
    }
    strcpy(f->name, pathname);

    atomic_init(&f->refcnt, 1);
    f->fd = fd;
    f->size = s.st_size;
    f->data = p;

    *file = f;

    log_ctx_pop();
    return 0;
}


struct mfile * mfile_get(struct mfile *file)
{
    assert(file != NULL);
    assert(atomic_load_explicit(&file->refcnt, memory_order_relaxed) > 0);
    atomic_fetch_add_explicit(&file->refcnt, 1, memory_order_relaxed);
    return file;
}


void mfile_put(struct mfile *file)
{
    assert(file != NULL);
    assert(atomic_load_explicit(&file->refcnt, memory_order_acquire) > 0);

    if (atomic_fetch_sub_explicit(&file->refcnt, 1, memory_order_acq_rel) == 1) {
        log_ctx_new(file->name);

        munmap((void*) file->data, file->size);
        close(file->fd);
        log_trace("File closed");

        log_ctx_pop();

        free(file->name);
        free(file);

    }
}
