#ifndef BFLD_WRITER_H
#define BFLD_WRITER_H
#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include "image.h"


/*
 * File writer back-end operations.
 *
 * Writes image content to file for a specific format.
 *
 * bfld can support different back-ends for writing executables
 * in different formats, i.e., ELF, Mach-O, PE/COFF, etc.
 */
struct writer
{
    /*
     * The name of the file writer.
     */
    const char *name;

    int (*prepare_file_layout)(struct image *image);
};


/*
 * Register a file format writer back-end.
 */
void writer_register(const struct writer *backend);


const struct writer * writer_lookup(const char *name);


#ifdef __cplusplus
}
#endif
#endif
