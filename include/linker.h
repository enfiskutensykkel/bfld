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
    char *name;                     // linker name (used for debugging, NOTE: can be NULL)
    int log_ctx;                    // log context
    struct list_head unprocessed;   // list of input files that should be processed
    struct list_head processed;     // list of input files that are finished processed
    struct list_head archives;      // list of archive files
    struct globals *globals;        // global symbols
};


/*
 * Input file that should be processed by the linker.
 */
struct input_file
{
    char *name;                     // filename
    struct linkerctx *ctx;          // weak reference to linker context
    struct list_head list_entry;    // linked list entry
    struct sections *sections;      // sections in this input file
    struct symbols *symbols;        // local symbols in this file
};


/*
 * Archive file that may provide symbols the linker need.
 */
struct archive_file
{
    struct linkerctx *ctx;          // weak reference to the linker context
    struct list_head list_entry;    // linker list entry
    struct archive *archive;        // archive file handle
};


/*
 * Create linker context.
 */
struct linkerctx * linker_create(const char *name);


/*
 * Tear down linker context.
 */
void linker_destroy(struct linkerctx *ctx);


/*
 * Add an archive file to the list of archives.
 */
struct archive_file * linker_add_archive(struct linkerctx *ctx,
                                         struct archive *archive,
                                         const struct archive_frontend *frontend);


/*
 * Add an object file to the input file list.
 */
struct input_file * linker_add_input_file(struct linkerctx *ctx,
                                          struct objfile *objfile,
                                          const struct objfile_frontend *frontend);


/*
 * Load file and add it to the input file list (or archive list)
 */
bool linker_load_file(struct linkerctx *ctx, const char *pathname);



/*
 * Resolve all global symbols.
 */
bool linker_resolve_globals(struct linkerctx *ctx);



#ifdef __cplusplus
}
#endif
#endif
