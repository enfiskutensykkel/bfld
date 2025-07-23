#ifndef __BFLD_MEMORY_MAPPED_FILE_H__
#define __BFLD_MEMORY_MAPPED_FILE_H__

#include <stddef.h>


/*
 * Memory-mapped file handle.
 *
 * Represents a read-only file descriptor that has been memory-mapped 
 * to allow convenient read access to the file.
 *
 * Holds the opened file descriptor, the memory-mapped pointer,
 * the size of the file, and also the path to the file.
 *
 * The handle is reference counted.
 */
typedef struct _mfile
{
    int refcnt;
    int fd;
    size_t size;
    const void *data;
    char name[];
} mfile;


/*
 * Open a file as read-only and memory-map its content
 * and allocate a new file handle.
 */
int mfile_init(mfile **fhandle, const char *pathname);


/* 
 * Increase the reference counter.
 */
void mfile_get(mfile *file);


/*
 * Decrease the reference counter.
 *
 * When the reference count is 0, the mapped memory is unmapped
 * and the file descriptor is closed and the handle is freed.
 *
 * Your code should not use the handle after calling this function.
 */
void mfile_put(mfile *file);


#endif
