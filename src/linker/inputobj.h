#ifndef __BFLD_INPUT_OBJECT_FILE_H__
#define __BFLD_INPUT_OBJECT_FILE_H__

#include "io.h"
#include <stddef.h>
#include <stdint.h>
#include <utils/list.h>


/*
 * Represents an object file given as input to the linker.
 * Holds a list of input sections.
 */
struct input_objfile
{
    const void *content;            // Pointer to the start of the object file
    size_t size;                    // Total size of the object file
    struct list_head entry;         // Linked list entry
    struct list_head sects;         // List of all input sections
};


/*
 * Input section from an object file.
 */
struct input_sect
{
    struct input_objfile *objfile;  // Object file this section came from
    struct list_head entry;         // Linked list entry
    uint32_t idx;                   // Section index
    size_t size;                    // Section size
    uint64_t offset;                // Offset from start of object file
    const void *content;            // Pointer to data
    // TODO: Add members (symbols etc)
};


/*
 * Read object file(s) from an opened input file.
 *
 * Will call the underlying system-specific function
 * for parsing object files and add parsed object files 
 * to the linked list pointed to by objfiles.
 */
int input_objfile_get_all(const struct ifile *fp, struct list_head *objfiles);


/*
 * Helper function to allocate an input object file.
 */
struct input_objfile * input_objfile_alloc(const void *content, size_t size);


/*
 * Release input object file.
 */
void input_objfile_free(struct input_objfile *objfile);


/*
 * Release all object files in the list.
 */
void input_objfile_put_all(struct list_head *objfiles);


/*
 * Helper function to allocate and add a section to the object file.
 */
struct input_sect * input_sect_alloc(struct input_objfile *objfile, 
                                     uint32_t idx,
                                     uint64_t offset,
                                     const void *content, 
                                     size_t size);

#endif
