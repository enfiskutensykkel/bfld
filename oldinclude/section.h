#ifndef __BFLD_SECTION_H__
#define __BFLD_SECTION_H__
#ifdef __cplusplus
extern "C" {
#endif

#include "secttypes.h"
#include <stddef.h>
#include <stdint.h>


/* Forward declaration of object file */
struct objfile;


/*
 * Allocate and initialize a section.
 *
 * This is a low level function which you probably 
 * should not be calling directly.
 *
 * Instead see objfile_get_section()
 */
int section_init(struct section **sect, 
                 struct objfile *objfile, 
                 uint64_t sect_key,
                 const char *name);


/*
 * Take a section reference (increase reference counter).
 */
void section_get(struct section *sect);


/*
 * Release a section reference (decrease reference counter).
 */
void section_put(struct section *sect);


#ifdef __cplusplus
}
#endif
#endif
