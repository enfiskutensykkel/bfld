#ifndef _BFLD_OBJECT_FILE_H
#define _BFLD_OBJECT_FILE_H
#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include "utils/rbtree.h"
#include "utils/list.h"


/* Some forward declarations */
struct mfile;


/*
 * Object file handle.
 * Holds a reference to the underlying object file where
 * sections, symbols and relocations are defined.
 */
struct objfile
{
    char *name;                 // name of the object file (NOTE: may be NULL)
    struct mfile *file;         // strong reference to the underlying memory mapped file
    int refcnt;                 // reference counter
    const uint8_t *file_data;   // pointer to the start of the file
    size_t file_size;           // total size of the file
};


/*
 * Allocate an object file handle.
 *
 * This takes a strong reference to the underlying 
 * memory mapped file.
 *
 * If content is NULL, the start of the file (mfile)
 * is used.
 */
struct objfile * objfile_alloc(struct mfile *file,
                               const char *name,
                               const uint8_t *file_data,
                               size_t file_size);

/*
 * Take a strong object file handle reference.
 * Increases the object file handle's reference counter.
 */
struct objfile * objfile_get(struct objfile *objfile);


/*
 * Release object file handle reference.
 * Decreases the object file handle reference counter.
 * When the reference count reaches zero, the underlying 
 * memory mapped file is released.
 */
void objfile_put(struct objfile *objfile);


#ifdef __cplusplus
}
#endif
#endif
