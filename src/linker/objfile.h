#ifndef __BFLD_OBJECT_FILE_H__
#define __BFLD_OBJECT_FILE_H__

#include "mfile.h"
#include <stddef.h>
#include <stdint.h>
#include <utils/list.h>



/*
 * Represents an object file given as input to the linker.
 */
struct objfile
{
    mfile *file;                    // File handle reference
    int refcnt;                     // Reference counter
    const void *file_data;          // Pointer to the start of the object file
    size_t file_size;               // Total size of the object file
    struct list_head entry;         // Linked list entry
    char name[];                    // Filename of the object file
};


/*
 * Helper function to allocate an input object file reference.
 *
 * The start pointer and size arguments indicates the offset into 
 * the memory-mapped file where the object file content begins. 
 * If this is set to NULL, then the pointer is assumed to be the 
 * start of the memory-mapped file and the size argument is ignored.
 *
 * If the object file has a different name than the memory-mapped
 * file, e.g., if it comes from a library or an archive file, then
 * this can be specified with the name argument.
 */
struct objfile * objfile_alloc(mfile *file, 
                               const void *start, 
                               size_t size,
                               const char *name);


/*
 * Acquire an object file reference.
 */
void objfile_get(struct objfile *objfile);


/*
 * Release an object file reference.
 */
void objfile_put(struct objfile *objfile);


/*
 * Load all object files found in the specified memory-mapped file
 * and add them to the list.
 */
int objfile_load(struct list_head *objfiles, mfile *file);


#define list_for_each_objfile(iterator, head_ptr) \
    list_for_each_node(iterator, head_ptr, struct objfile, entry)


#define list_objfile(head_ptr) list_node(head_ptr, struct objfile, entry)


#endif
