#ifndef __BFLD_ARCHIVE_FILE_H__
#define __BFLD_ARCHIVE_FILE_H__

#include "mfile.h"
#include "utils/list.h"
#include <stdbool.h>


/*
 * Check if the first bytes of the memory-mapped
 * area contains the magic archive signature.
 */
bool ar_check_magic(const void*);


/*
 * Parse archive file and extract object files.
 */
int ar_parse_members(mfile *fp, struct list_head *objfiles);



/*
 * Helper function to lookup a symbol from the archive index, if it is present.
 */
const void * ar_lookup_symbol(const mfile *fp, const char *symname);

#endif
