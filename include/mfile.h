#ifndef __BFLD_MEMORY_MAPPED_FILE_H__
#define __BFLD_MEMORY_MAPPED_FILE_H__
#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>


/*
 * Memory-mapped file handle.
 *
 * Represents a read-only file descriptor that has been memory-mapped,
 * to allow convenient and fast read access to the file.
 */
struct _mfile
{
    char *name;         // filename used when opening the file
    int refcnt;         // reference counter
    int fd;             // the file descriptor used to open the file
    size_t size;        // the total size of the file
    const void *data;   // memory mapped pointer to the start of the file
};

typedef struct _mfile mfile;


/*
 * Open a file as read-only and memory-map its content
 * and allocate a new file handle.
 */
int mfile_open_read(mfile **fhandle, const char *pathname);


/* 
 * Increase the reference counter.
 */
void mfile_get(mfile *file);


/*
 * Decrease the reference counter.
 *
 * When the reference count is 0, the mapped memory is unmapped
 * and the file descriptor is closed and the handle is freed.
 */
void mfile_put(mfile *file);


#ifdef __cplusplus
}
#endif
#endif
