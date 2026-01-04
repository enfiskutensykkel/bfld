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
struct secttab;
struct symtab;
struct objfile;
struct objfile_frontend;
struct archive;
struct archive_frontend;


/* 
 * Global linker context.
 */
struct linkerctx
{
    int log_ctx_idx;
    char *name; // used for debugging
    struct list_head input_files;
    struct list_head archives;
    struct secttab *input_sects;
    struct symtab *symtab;
};


struct input_file
{
    struct linkerctx *ctx;
    struct list_head list_entry;
    struct objfile *objfile;
    struct secttab *sections;
    const struct objfile_frontend *frontend;
};


struct archive_file
{
    struct linkerctx *ctx;
    struct list_head list_entry;
    struct archive *archive;
    const struct archive_frontend *frontend;
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
