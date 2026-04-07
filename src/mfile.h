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
    _Atomic(uint32_t) refcnt;   // Reference counter
    struct mfile *parent;       // parent file pointer
    char *name;                 // filename used when opening the file
    int fd;                     // the underlying file descriptor used to open the file
    size_t size;                // total size of the file in memory
    uint8_t *data;              // memory-mapped pointer to the start of file contents
};


/*
 * Open input file as memory map its content.
 */
struct mfile * mfile_open(const char *pathname);


/*
 * Take a reference to the memory mapped file.
 */
struct mfile * mfile_get(struct mfile *file);


/*
 * Release the memory mapped file reference.
 */
void mfile_put(struct mfile *file);


/*
 * Get the offset of the file from the start of the parent file.
 */
static inline
size_t mfile_offset(const struct mfile *file)
{
    if (file->parent != NULL) {
        return (file->data - file->parent->data) + mfile_offset(file->parent);
    }
    return 0;
}


/*
 * Get the filename of the memory mapped file.
 */
static inline
const char * mfile_name(const struct mfile *file)
{
    return file->name;
}


#ifdef __cplusplus
}
#endif
#endif
