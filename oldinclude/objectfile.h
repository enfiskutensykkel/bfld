#ifndef BFLD_OBJECT_FILE_H
#define BFLD_OBJECT_FILE_H
#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>


/* Some forward declarations */
struct mfile;
struct sections;
struct symbols;
struct objectfile_reader;


/*
 * Object file handle.
 * Holds a reference to the underlying object file where
 * sections, symbols and relocations are defined.
 */
struct objectfile
{
    char *name;                 // name of the object file
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
 * If content is NULL, the start of the memory
 * mapped file (mfile) is used as the content pointer.
 */
struct objectfile * objectfile_alloc(struct mfile *file,
                                     const char *name,
                                     const uint8_t *file_data,
                                     size_t file_size);

/*
 * Take a strong object file handle reference.
 * Increases the object file handle's reference counter.
 */
struct objectfile * objectfile_get(struct objectfile *file);


/*
 * Release object file handle reference.
 * Decreases the object file handle reference counter.
 * When the reference count reaches zero, the underlying 
 * memory mapped file is released.
 */
void objectfile_put(struct objectfile *file);


/*
 * Load sections and symbols from an object file.
 *
 * If an object file reader is specified, i.e., frontend is not NULL, 
 * that specific front-end will be used to read the file. 
 * Otherwise this function will try to look up which front-end to use.
 *
 * Returns zero on success and an errno on failure indicating
 * the error:
 * - EIO if the file can not be parsed
 * - ENOMEM if allocating internal memory structures failed
 */
int objectfile_load(struct objectfile *file,
                    struct sections *sections,
                    struct symbols *symbols);

#ifdef __cplusplus
}
#endif
#endif
