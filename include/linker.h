#ifndef _BFLD_LINKER_CONTEXT_H
#define _BFLD_LINKER_CONTEXT_H
#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "utils/list.h"

/* Some forward declarations */
struct sections;
struct globals;
struct symbols;
struct objfile;
struct objfile_frontend;
struct archive;
struct archive_frontend;


/* 
 * Linker context.
 */
struct linkerctx
{
    char *name;                     // linker name (used for debugging)
    int log_ctx;                    // log context
    struct list_head input_files;   // list of input files that should be processed
    struct list_head archives;      // list of archive files
    struct globals *globals;        // global symbols
};


/*
 * Input file that should be processed by the linker.
 */
struct input_file
{
    struct linkerctx *ctx;          // weak reference to linker context
    struct list_head list_entry;    // linked list entry
    struct objfile *objfile;        // object file handle
    struct sections *sections;      // sections in this input file
    struct symbols *symbols;        // local symbols in this file
    const struct objfile_frontend *frontend; // frontend used for loading this file
};


/*
 * Archive file that may provide symbols the linker need.
 */
struct archive_file
{
    struct linkerctx *ctx;          // weak reference to the linker context
    struct list_head list_entry;    // linker list entry
    struct archive *archive;        // archive file handle
    const struct archive_frontend *frontend; // frontend used for loading this file
};


/*
 * Create linker context.
 */
struct linkerctx * linker_create(const char *name);


/*
 * Tear down linker context.
 */
void linker_destroy(struct linkerctx *ctx);


struct archive_file * linker_add_archive(struct linkerctx *ctx,
                                         struct archive *archive,
                                         const struct archive_frontend *frontend);


struct input_file * linker_add_objfile(struct linkerctx *ctx,
                                       struct objfile *objfile,
                                       const struct objfile_frontend *frontend);


bool linker_load_file(struct linkerctx *ctx, const char *pathname);


#ifdef __cplusplus
}
#endif
#endif
