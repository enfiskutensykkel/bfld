#include "logging.h"
#include "linker.h"
#include "objfile.h"
#include "objfile_frontend.h"
#include "archive_frontend.h"
#include "utils/list.h"
#include "secttab.h"
#include "symtab.h"
#include "mfile.h"
#include "archive.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>


/*
 * Archive file front-end entry.
 */
struct archive_fe_entry
{
    struct list_head node;
    const struct archive_frontend *frontend;
};


/*
 * Object file front-end entry
 */
struct objfile_fe_entry
{
    struct list_head node;
    const struct objfile_frontend *frontend;
};



int log_verbosity = 1;
int __log_ctx_idx = 0;
log_ctx_t __log_ctx[LOG_CTX_NUM] = {0};

static struct list_head objfile_frontends = LIST_HEAD_INIT(objfile_frontends);

static struct list_head archive_frontends = LIST_HEAD_INIT(archive_frontends);


void archive_frontend_register(const struct archive_frontend *fe)
{
    if (fe == NULL || fe->name == NULL) {
        return;
    }

    if (fe->probe_file == NULL || fe->parse_file == NULL) {
        return;
    }

    struct archive_fe_entry *entry = malloc(sizeof(struct archive_fe_entry));
    if (entry == NULL) {
        return;
    }

    entry->frontend = fe;
    list_insert_tail(&archive_frontends, &entry->node);
}


void objfile_frontend_register(const struct objfile_frontend *fe)
{
    if (fe == NULL || fe->name == NULL) {
        return;
    }

    if (fe->probe_file == NULL || fe->parse_file == NULL) {
        return;
    }

    struct objfile_fe_entry *entry = malloc(sizeof(struct objfile_fe_entry));
    if (entry == NULL) {
        return;
    }

    entry->frontend = fe;
    list_insert_tail(&objfile_frontends, &entry->node);
}


/*
 * Remove all registered front-ends
 */
__attribute__((destructor(65535)))
static void remove_frontends(void)
{
    list_for_each_entry_safe(entry, &objfile_frontends, struct objfile_fe_entry, node) {
        list_remove(&entry->node);
        free(entry);
    }

    list_for_each_entry_safe(entry, &archive_frontends, struct archive_fe_entry, node) {
        list_remove(&entry->node);
        free(entry);
    }
}


const struct objfile_frontend * objfile_frontend_probe(const uint8_t *data, size_t size)
{
    list_for_each_entry(entry, &objfile_frontends, struct objfile_fe_entry, node) {
        const struct objfile_frontend *fe = entry->frontend;

        if (fe->probe_file(data, size)) {
            return fe;
        }
    }
    return NULL;
}


const struct archive_frontend * archive_frontend_probe(const uint8_t *data, size_t size)
{
    list_for_each_entry(entry, &archive_frontends, struct archive_fe_entry, node) {
        const struct archive_frontend *fe = entry->frontend;

        if (fe->probe_file(data, size)) {
            return fe;
        }
    }
    return NULL;
}


struct linkerctx * linker_create(const char *name)
{
    log_ctx_push(LOG_CTX(name));

    struct linkerctx *ctx = malloc(sizeof(struct linkerctx));
    if (ctx == NULL) {
        log_fatal("Unable to create linker context");
        log_ctx_pop();
        return NULL;
    }

    ctx->name = strdup(name);
    if (ctx->name == NULL) {
        log_fatal("Unable to create linker context");
        free(ctx);
        log_ctx_pop();
        return NULL;
    }

    ctx->log_ctx_idx = __log_ctx_idx;
    list_head_init(&ctx->input_files);
    list_head_init(&ctx->archives);
    ctx->input_sects = NULL;
    ctx->symtab = NULL;
    return ctx;
}


void linker_destroy(struct linkerctx *ctx)
{
    if (ctx != NULL) {
        assert(__log_ctx_idx > 0);
        assert(__log_ctx_idx == ctx->log_ctx_idx);

        // This should not happen
        while (__log_ctx_idx > ctx->log_ctx_idx) {
            log_warning("Unwinding log context stack");
            log_ctx_pop();
        }

        list_for_each_entry_safe(file, &ctx->input_files, struct input_file, list_entry) {
            objfile_put(file->objfile);
            list_remove(&file->list_entry);
            secttab_put(file->sections);
            free(file);
        }

        list_for_each_entry_safe(arfile, &ctx->archives, struct archive_file, list_entry) {
            archive_put(arfile->archive);
            list_remove(&arfile->list_entry);
            free(arfile);
        }

        free(ctx->name);
        free(ctx);
        log_ctx_pop();
    }
}


struct archive_file * linker_add_archive(struct linkerctx *ctx, struct archive *ar, 
                                         const struct archive_frontend *fe)
{

    if (fe == NULL) {
        fe = archive_frontend_probe(ar->file_data, ar->file_size);
    }

    if (fe == NULL) {
        log_fatal("Unrecognized file format");
        return NULL;
    }

    log_debug("Using front-end '%s' for archive file", fe->name);
    int status = fe->parse_file(ar->file_data, ar->file_size, ar);
    if (status != 0) {
        log_fatal("Unable to parse archive file");
        return NULL;
    }

    struct archive_file *arfile = malloc(sizeof(struct archive_file));
    if (arfile == NULL) {
        log_fatal("Unable to allocate file handle");
        return NULL;
    }

    arfile->ctx = ctx;
    list_insert_tail(&ctx->archives, &arfile->list_entry);
    arfile->archive = archive_get(ar);
    arfile->frontend = fe;
    
    log_trace("Successfully parsed archive file");
    return arfile;
}


struct input_file * linker_add_objfile(struct linkerctx *ctx,
                                       struct objfile *objfile,
                                       const struct objfile_frontend *fe)
{
    if (fe == NULL) {
        fe = objfile_frontend_probe(objfile->file_data, objfile->file_size);
    }

    if (fe == NULL) {
        log_fatal("Unrecognized file format");
        return NULL;
    }

    struct input_file *file = malloc(sizeof(struct input_file));
    if (file == NULL) {
        log_fatal("Unable to allocate file handle");
        return NULL;
    }

    file->ctx = ctx;
    file->objfile = objfile_get(objfile);
    file->sections = secttab_alloc(objfile->name);
    file->frontend = fe;

    log_debug("Using front-end '%s' for object file", fe->name);
    int status = fe->parse_file(objfile->file_data, objfile->file_size, 
                                objfile, file->sections, ctx->symtab);
    if (status != 0) {
        log_fatal("Unable to parse object file");
        objfile_put(file->objfile);
        free(file);
        return NULL;
    }

    list_insert_tail(&ctx->input_files, &file->list_entry);
    log_trace("Successfully parsed object file");
    return file;
}


bool linker_load_file(struct linkerctx *ctx, const char *pathname)
{
    bool success = false;
    struct mfile *file = NULL;

    log_ctx_push(LOG_CTX_FILE(NULL, pathname));

    int status = mfile_open_read(&file, pathname);
    if (status != 0) {
        log_fatal("Could not open file");
        log_ctx_pop();
        return false;
    }

    // Try to open as archive first
    list_for_each_entry(entry, &archive_frontends, struct archive_fe_entry, node) {
        const struct archive_frontend *fe = entry->frontend;

        if (!fe->probe_file(file->data, file->size)) {
            continue;
        }

        struct archive *ar = archive_alloc(file, file->name, file->data, file->size);
        if (ar == NULL) {
            goto unwind;
        }

        struct archive_file * arfile = linker_add_archive(ctx, ar, fe);
        archive_put(ar);
        if (arfile == NULL) {
            goto unwind;
        }

        success = true;
        goto unwind;
    }

    // Try to open as an object file
    list_for_each_entry(entry, &objfile_frontends, struct objfile_fe_entry, node) {
        const struct objfile_frontend *fe = entry->frontend;

        if (!fe->probe_file(file->data, file->size)) {
            continue;
        }

        struct objfile *obj = objfile_alloc(file, file->name, file->data, file->size);
        if (obj == NULL) {
            goto unwind;
        }

        struct input_file * infile = linker_add_objfile(ctx, obj, fe);
        objfile_put(obj);
        if (infile == NULL) {
            goto unwind;
        }

        success = true;
        goto unwind;
    }

    log_fatal("Unrecognized file format");

unwind:
    mfile_put(file);
    log_ctx_pop();
    return success;
}

