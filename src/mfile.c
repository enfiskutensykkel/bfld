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


struct mfile * mfile_open(const char *pathname)
{
    log_ctx_new(pathname);

    int fd = open(pathname, O_RDONLY);
    if (fd == -1) {
        log_error("Unable to open file");
        log_ctx_pop();
        return NULL;
    }

    // Get the file size so that the entire file can be memory-mapped
    struct stat s;
    if (fstat(fd, &s) == -1) {
        close(fd);
        log_error("Unable to check file size");
        log_ctx_pop();
        return NULL;
    }

    // Memory-map the file
    // Uses MAP_PRIVATE for copy-on-write
    void *p = mmap(NULL, s.st_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
    if (p == MAP_FAILED) {
        close(fd);
        log_error("Unable to memory-map file");
        log_ctx_pop();
        return NULL;
    }

    log_trace("File opened");

    // Create file handle
    struct mfile *file = malloc(sizeof(struct mfile));
    if (file == NULL) {
        munmap(p, s.st_size);
        close(fd);
        log_ctx_pop();
        return NULL;
    }

    file->name = malloc(strlen(pathname) + 1);
    if (file->name == NULL) {
        free(file);
        munmap(p, s.st_size);
        close(fd);
        log_ctx_pop();
        return NULL;
    }
    strcpy(file->name, pathname);

    file->parent = NULL;
    atomic_init(&file->refcnt, 1);
    file->fd = fd;
    file->size = s.st_size;
    file->data = p;

    log_ctx_pop();
    return file;
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

        if (file->parent == NULL) {
            munmap((void*) file->data, file->size);
            close(file->fd);
        } else {
            mfile_put(file->parent);
        }
        log_trace("File closed");

        log_ctx_pop();

        free(file->name);
        free(file);

    }
}
