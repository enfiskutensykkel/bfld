#include "logging.h"
#include "linker.h"
#include "objfile.h"
#include "frontends/objfile.h"
#include "frontends/archive.h"
#include "utils/list.h"
#include "sections.h"
#include "globals.h"
#include "mfile.h"
#include "archive.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>


/*
 * Archive file front-end entry.
 * Used to track registered front-ends.
 */
struct archive_fe_entry
{
    struct list_head node;
    const struct archive_frontend *frontend;
};


/*
 * Object file front-end entry
 * Used to track registered front-ends.
 */
struct objfile_fe_entry
{
    struct list_head node;
    const struct objfile_frontend *frontend;
};



int log_level = 1;  // initial log level
int log_ctx = 0;    // initial log context
log_ctx_t log_ctx_stack[LOG_CTX_MAX] = {0};


/*
 * List of object file front-ends.
 */ 
static struct list_head objfile_frontends = LIST_HEAD_INIT(objfile_frontends);


/*
 * List of archive file front-ends.
 */
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
    int log_ctx = log_ctx_new("");

    struct linkerctx *ctx = malloc(sizeof(struct linkerctx));
    if (ctx == NULL) {
        log_ctx_pop();
        return NULL;
    }

    ctx->name = strdup(name);
    if (ctx->name == NULL) {
        free(ctx);
        log_ctx_pop();
        return NULL;
    }

    ctx->globals = globals_alloc("gsym");
    if (ctx->globals == NULL) {
        free(ctx->name);
        free(ctx);
        log_ctx_pop();
        return NULL;
    }

    ctx->log_ctx = log_ctx;
    list_head_init(&ctx->input_files);
    list_head_init(&ctx->archives);
    return ctx;
}


void linker_destroy(struct linkerctx *ctx)
{
    if (ctx != NULL) {
        assert(log_ctx > 0);
        assert(log_ctx == ctx->log_ctx);

        // This should not happen
        while (log_ctx > ctx->log_ctx) {
            log_warning("Unwinding log context stack");
            log_ctx_pop();
        }

        list_for_each_entry_safe(file, &ctx->input_files, struct input_file, list_entry) {
            objfile_put(file->objfile);
            list_remove(&file->list_entry);
            sections_put(file->sections);
            symbols_put(file->symbols);
            free(file);
        }

        list_for_each_entry_safe(arfile, &ctx->archives, struct archive_file, list_entry) {
            archive_put(arfile->archive);
            list_remove(&arfile->list_entry);
            free(arfile);
        }

        globals_put(ctx->globals);
        free(ctx->name);
        free(ctx);
        log_ctx_pop();
    }
}


struct archive_file * linker_add_archive(struct linkerctx *ctx, struct archive *ar, 
                                         const struct archive_frontend *fe)
{
    log_ctx_new(ar->name);

    if (fe == NULL) {
        fe = archive_frontend_probe(ar->file_data, ar->file_size);
    }

    if (fe == NULL) {
        log_fatal("Unrecognized file format");
        log_ctx_pop();
        return NULL;
    }
    log_trace("Front-end '%s' is best match for archive", fe->name);

    int status = fe->parse_file(ar->file_data, ar->file_size, ar);
    if (status != 0) {
        log_ctx_pop();
        return NULL;
    }

    struct archive_file *arfile = malloc(sizeof(struct archive_file));
    if (arfile == NULL) {
        log_fatal("Unable to allocate file handle");
        log_ctx_pop();
        return NULL;
    }

    arfile->ctx = ctx;
    list_insert_tail(&ctx->archives, &arfile->list_entry);
    arfile->archive = archive_get(ar);
    arfile->frontend = fe;
    
    log_trace("Successfully loaded archive file");
    log_ctx_pop();
    return arfile;
}


struct input_file * linker_add_objfile(struct linkerctx *ctx,
                                       struct objfile *objfile,
                                       const struct objfile_frontend *fe)
{
    log_ctx_new(objfile->name);
    if (fe == NULL) {
        fe = objfile_frontend_probe(objfile->file_data, objfile->file_size);
    }

    if (fe == NULL) {
        log_ctx_pop();
        return NULL;
    }
    log_trace("Front-end '%s' is best match for object file", fe->name);

    struct input_file *file = malloc(sizeof(struct input_file));
    if (file == NULL) {
        log_ctx_pop();
        return NULL;
    }

    file->symbols = symbols_alloc("symboltable");
    if (file->symbols == NULL) {
        free(file);
        log_ctx_pop();
        return NULL;
    }

    file->sections = sections_alloc("sectiontable");
    if (file->sections == NULL) {
        symbols_put(file->symbols);
        free(file);
        log_ctx_pop();
        return NULL;
    }

    file->ctx = ctx;
    file->objfile = objfile_get(objfile);
    

    file->frontend = fe;

    int status = fe->parse_file(objfile->file_data, objfile->file_size, 
                                objfile, file->sections, file->symbols, ctx->globals);
    if (status != 0) {
        symbols_put(file->symbols);
        objfile_put(file->objfile);
        sections_put(file->sections);
        free(file);
        log_ctx_pop();
        return NULL;
    }

    list_insert_tail(&ctx->input_files, &file->list_entry);
    log_trace("Successfully loaded object file");
    log_ctx_pop();
    return file;
}


bool linker_load_file(struct linkerctx *ctx, const char *pathname)
{
    bool success = false;
    struct mfile *file = NULL;

    log_ctx_new(pathname);

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

