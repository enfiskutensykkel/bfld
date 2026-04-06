#ifndef BFLD_MEMORY_MAPPED_FILE_H
#define BFLD_MEMORY_MAPPED_FILE_H
#ifdef __cplusplus
extern "C" {
#endif

#include "cdefs.h"
#include <stddef.h>
#include <stdint.h>


/*
 * Handle for a memory-mapped input file.
 * Uses reference counting to deal with when it is
 * safe to unmap the file and close the file descriptor.
 */
struct mfile
{
    _Atomic(uint32_t) refcnt;   // reference counter
    char *name;                 // filename used when opening the file
    int fd;                     // the underlying file descriptor used to open the file
    size_t size;                // total size of the file in memory
    const uint8_t *data;        // memory-mapped pointer to the start of file contents
};


/*
 * Open a file as read-only and memory map its content.
 */
int mfile_open(struct mfile **file, const char *pathname);


/*
 * Take a reference to the memory mapped file.
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
