#ifndef __BFLD_OBJECT_FILE_H__
#define __BFLD_OBJECT_FILE_H__
#ifdef __cplusplus
extern "C" {
#endif

#include "mfile.h"
#include "utils/rbtree.h"
#include <stddef.h>
#include <stdint.h>


/*
 * Representation of an object file.
 * Object files are input files to the linker.
 */
struct objfile
{
    char *name;             // filename used when opening the object file
    int refcnt;             // reference counter
    mfile *file;            // reference to the underlying memory-map
    void *loader_data;      // private data for the object file loader
    const struct objfile_loader *loader;  // the underlying object file loader (front-end)
};


/*
 * Allocate and initialize a object file handle.
 * 
 * This is a low level function which you probably 
 * should not be calling directly.
 *
 * Instead see objfile_load()
 */
int objfile_init(struct objfile **objfile, mfile *file, const char *name);


/*
 * Take an object file reference (increase reference counter).
 */
void objfile_get(struct objfile *objfile);


/*
 * Release an object file reference (decrease reference counter).
 */
void objfile_put(struct objfile *objfile);


/*
 * Attempt load the specified file as an object file.
 *
 * If a loader is specified, this function will try to use that specific
 * loader to load the file. If loader is NULL, registered loaders are attempted
 * one by one until either an appropriate file loader is found or there are 
 * no more loaders to try (and NULL is returned).
 */
struct objfile * objfile_load(mfile *file, const struct objfile_loader *loader);



/*
 * Extract all symbols from the object file.
 */
int objfile_extract_symbols(struct objfile* objfile,
                            struct rb_tree *global_symtab,
                            struct rb_tree *local_symtab);



//uint64_t objfile_num_sections(const struct objfile *objfile);
//struct section * objfile_exctract_section(const struct objfile *objfile, uint64_t sect_idx);


#ifdef __cplusplus
}
#endif
#endif
