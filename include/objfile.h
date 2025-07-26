#ifndef __BFLD_OBJECT_FILE_H__
#define __BFLD_OBJECT_FILE_H__
#ifdef __cplusplus
extern "C" {
#endif

#include "utils/list.h"
#include <stddef.h>
#include <stdint.h>


/*
 * Forward declaration of a section.
 */
struct section;


/*
 * Forward declaration of a declared symbol.
 */
struct symbol;


/*
 * Representation of an input file, or more specifically, an input unit.
 */
struct objfile;


/*
 * Forward declaration of an object file loader.
 */
struct objfile_loader;



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
 */
const char * objfile_filename(const struct objfile *objfile);


/*
 * Get the name of the object file loader used to parse this object file.
 */
const char * objfile_loader_name(const struct objfile *objfile);


/*
 * Get the loader used to parse this object file.
 */
const struct objfile_loader * objfile_get_loader(const struct objfile *objfile);


///*
// * Get the number of sections the object file has.
// */
//uint64_t objfile_num_sections(const struct objfile *objfile);
//
//
///*
// * Create a new section reference 
// */
//struct section * objfile_exctract_section(const struct objfile *objfile, uint64_t sect_idx);


#ifdef __cplusplus
}
#endif
#endif
