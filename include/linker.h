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
struct section;
struct globals;
struct symbols;
struct objfile;
struct objfile_frontend;
struct archive;
struct archive_frontend;
struct backend;


/* 
 * Linker context.
 */
struct linkerctx
{
    char *name;                     // linker name (used for debugging, NOTE: can be NULL)
    int log_ctx;                    // log context
    uint32_t march;                 // machine code architecture
    struct list_head archives;      // list of archive files
    struct globals *globals;        // global symbols
    struct sections *sections;      // global list of input sections
    struct sections *keep;          // list of sections to keep
    struct symbols *unresolved;     // list of unresolved symbols
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
                        const struct archive_frontend *frontend);


/*
 * Add an object file to the input file list.
 */
bool linker_add_input_file(struct linkerctx *ctx,
                           struct objfile *objfile,
                           const struct objfile_frontend *frontend);


/*
 * Resolve all undefined global symbols.
 */
bool linker_resolve_globals(struct linkerctx *ctx);


bool linker_create_common_section(struct linkerctx *ctx);


void linker_gc_sections(struct linkerctx *ctx);


void linker_keep_section(struct linkerctx *ctx, struct section *section);


#ifdef __cplusplus
}
#endif
#endif
