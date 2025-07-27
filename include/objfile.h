#ifndef __BFLD_OBJECT_FILE_H__
#define __BFLD_OBJECT_FILE_H__
#ifdef __cplusplus
extern "C" {
#endif

#include "mfile.h"
#include "utils/list.h"
#include "utils/rbtree.h"
#include <stddef.h>
#include <stdint.h>


/*
 * Representation of an input file, or more specifically, an input unit.
 */
struct objfile;


/*
 * Forward declaration of an object file loader.
 */
struct objfile_loader;


/*
 * Allocate and initialize a object file handle.
 * 
 * This is a low level function which you probably 
 * should not be calling directly.
 *
 * Instead see objfile_load()
 */
int objfile_init(struct objfile **objfile, const char *filename,
                 const uint8_t *data, size_t size);


/*
 * Take an object file reference.
 */
void objfile_get(struct objfile *objfile);


/*
 * Release an object file reference.
 */
void objfile_put(struct objfile *objfile);


/*
 * Get the filename of the object file.
 * May be NULL if unknown.
 */
const char * objfile_filename(const struct objfile *objfile);


/*
 * Get the underlying loader used to load this object file.
 * Can be NULL if the loader is not set or unknown.
 */
const struct objfile_loader * objfile_get_loader(const struct objfile *objfile);


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
