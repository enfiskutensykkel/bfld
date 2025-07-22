#ifndef __BFLD_ARCHIVE_FILE_H__
#define __BFLD_ARCHIVE_FILE_H__

#include "../io.h"


/*
 * Helper function to lookup a global symbol from the archive index, if it is present.
 */
const void * ar_lookup_gsym(const struct ifile *fp, const char *symname);

#endif
