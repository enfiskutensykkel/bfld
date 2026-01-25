#ifndef BFLD_LINKER_H
#define BFLD_LINKER_H
#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "utils/list.h"
#include "sections.h"
#include "symbols.h"

/* Some forward declarations */
struct globals;
struct objfile;
struct objfile_reader;
struct archive;
struct archive_reader;
struct backend;
struct image;


/* 
 * Linker context.
 */
struct linkerctx
{
    char *name;                     // linker name (used for debugging, NOTE: can be NULL)
    bool gc_sections;               // should we keep all sections and symbols?
    uint32_t target;                // machine code architecture target
    struct list_head archives;      // list of archive files
    struct globals *globals;        // global symbols
    struct sections sections;       // worklist of input sections
    struct symbols unresolved;      // worklist of unresolved symbols
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
bool linker_add_archive(struct linkerctx *ctx,
                        struct archive *archive,
                        const struct archive_reader *frontend);


/*
 * Add an object file to the input file list.
 */
bool linker_add_input_file(struct linkerctx *ctx,
                           struct objfile *objfile,
                           const struct objfile_reader *frontend);


/*
 * Resolve all undefined global symbols.
 */
bool linker_resolve_globals(struct linkerctx *ctx);


/*
 * Mark sections and symbols as alive.
 */
void linker_gc_sections(struct linkerctx *ctx, 
                        const struct sections *keep);


/*
 * Create a common section.
 */
bool linker_create_common_section(struct linkerctx *ctx);


#ifdef __cplusplus
}
#endif
#endif
