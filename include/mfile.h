#ifndef BFLD_MEMORY_MAPPED_FILE_H
#define BFLD_MEMORY_MAPPED_FILE_H
#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>


/*
 * Memory-mapped file handle.
 */
struct mfile
{
    char *name;         // filename used when opening the file
    int refcnt;         // reference counter
    int fd;             // the file descriptor used to open the file
    size_t size;        // total size of the file
    const void *data;   // memory-mapped pointer to the start of file contents
};


/*
 * Open a file as read-only and memory map its content.
 */
int mfile_open_read(struct mfile **file, const char *pathname);


/*
 * Take a strong reference to the memory mapped file.
 * Increases the reference counter.
 */
struct mfile * mfile_get(struct mfile *file);


/*
 * Release reference to the memory mapped file.
 * Decreasse the reference counter.
 *
 * When the reference count is 0, the mapped memory is unmapped
 * and the file descriptor is closed.
 */
void mfile_put(struct mfile *file);


#ifdef __cplusplus
}
#endif
#endif
