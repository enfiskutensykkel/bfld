#include "objfile_frontend.h"
#include "archive_frontend.h"
#include "logging.h"
#include "linker.h"
#include "objfile.h"
#include "utils/list.h"
#include "sections.h"
#include "symbols.h"
#include "symbol.h"
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


const struct objfile_frontend * objfile_frontend_probe(const uint8_t *data, size_t size, uint32_t *march)
{
    uint32_t m = 0;

    list_for_each_entry(entry, &objfile_frontends, struct objfile_fe_entry, node) {
        const struct objfile_frontend *fe = entry->frontend;

        if (fe->probe_file(data, size, &m)) {
            if (march != NULL) {
                *march = m;
            }
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

    if (name != NULL) {
        ctx->name = strdup(name);
    }

    ctx->globals = globals_alloc("globals");
    if (ctx->globals == NULL) {
        if (ctx->name != NULL) {
            free(ctx->name);
        }
        free(ctx);
        log_ctx_pop();
        return NULL;
    }

    ctx->log_ctx = log_ctx;
    list_head_init(&ctx->unprocessed);
    list_head_init(&ctx->processed);
    list_head_init(&ctx->archives);
    return ctx;
}


static void remove_input_file(struct input_file *file)
{
    list_remove(&file->list_entry);
    sections_put(file->sections);
    symbols_put(file->symbols);
    if (file->name != NULL) {
        free(file->name);
    }
    free(file);
}


static void remove_archive_file(struct archive_file *file)
{
    archive_put(file->archive);
    list_remove(&file->list_entry);
    free(file);
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

        list_for_each_entry_safe(file, &ctx->unprocessed, struct input_file, list_entry) {
            remove_input_file(file);
        }

        list_for_each_entry_safe(file, &ctx->processed, struct input_file, list_entry) {
            remove_input_file(file);
        }

        list_for_each_entry_safe(file, &ctx->archives, struct archive_file, list_entry) {
            remove_archive_file(file);
        }

        globals_put(ctx->globals);
        if (ctx->name != NULL) {
            free(ctx->name);
        }
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
        log_error("Unrecognized file format");
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
        log_ctx_pop();
        return NULL;
    }

    arfile->ctx = ctx;
    list_insert_tail(&ctx->archives, &arfile->list_entry);
    arfile->archive = archive_get(ar);
    
    log_trace("Archive file added");
    log_ctx_pop();
    return arfile;
}


struct input_file * linker_add_input_file(struct linkerctx *ctx,
                                          struct objfile *objfile,
                                          const struct objfile_frontend *fe)
{
    log_ctx_new(objfile->name);

    if (fe == NULL) {
        fe = objfile_frontend_probe(objfile->file_data, objfile->file_size, &objfile->march);
    }

    if (fe == NULL) {
        log_error("Unrecognized file format");
        log_ctx_pop();
        return NULL;
    }

    log_trace("Front-end '%s' is best match for object file", fe->name);

    struct input_file *file = malloc(sizeof(struct input_file));
    if (file == NULL) {
        log_ctx_pop();
        return NULL;
    }

    file->name = strdup(objfile->name);

    file->symbols = symbols_alloc(objfile->name);
    if (file->symbols == NULL) {
        free(file);
        log_ctx_pop();
        return NULL;
    }

    file->sections = sections_alloc(objfile->name);
    if (file->sections == NULL) {
        symbols_put(file->symbols);
        free(file);
        log_ctx_pop();
        return NULL;
    }

    file->ctx = ctx;
    
    int status = fe->parse_file(objfile->file_data, objfile->file_size, 
                                objfile, file->sections, file->symbols, ctx->globals);
    if (status != 0) {
        symbols_put(file->symbols);
        sections_put(file->sections);
        free(file);
        log_ctx_pop();
        return NULL;
    }

    list_insert_tail(&ctx->unprocessed, &file->list_entry);
    log_trace("Added object file to input files");
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
        uint32_t march = 0;

        if (!fe->probe_file(file->data, file->size, &march)) {
            continue;
        }

        struct objfile *obj = objfile_alloc(file, file->name, file->data, file->size);
        if (obj == NULL) {
            goto unwind;
        }
        obj->march = march;

        struct input_file * infile = linker_add_input_file(ctx, obj, fe);
        objfile_put(obj);
        if (infile == NULL) {
            goto unwind;
        }

        success = true;
        goto unwind;
    }

    log_error("Unrecognized file format");

unwind:
    mfile_put(file);
    log_ctx_pop();
    return success;
}


bool linker_resolve_globals(struct linkerctx *ctx)
{
    bool loaded_file = false;

    do {
        loaded_file = false;

        list_for_each_entry_safe(file, &ctx->unprocessed, struct input_file, list_entry) {

            log_ctx_new(file->name);

            for (size_t i = 0, n = 0; i < file->symbols->capacity && n < file->symbols->nsymbols; ++i) {
                const struct symbol *sym = symbols_at(file->symbols, i);

                if (sym == NULL) {
                    continue;
                }

                ++n;

                if (!symbol_is_defined(sym) && !sym->is_common) {
                    log_trace("Symbol '%s' is undefined", sym->name);

                    list_for_each_entry(ar, &ctx->archives, struct archive_file, list_entry) {
                        struct archive_member *m = archive_find_symbol(ar->archive, sym->name);

                        // No member provides the symbol
                        if (m == NULL) {
                            continue;
                        }

                        // Member provides the symbol, but it is already loaded
                        if (m->objfile != NULL) {
                            continue;
                        }

                        log_debug("Found symbol '%s' in archive, loading member file", sym->name);
                        struct objfile *obj = archive_get_objfile(m);
                        if (obj != NULL) {
                            loaded_file = linker_add_input_file(ctx, obj, NULL) != NULL;
                            objfile_put(obj);
                        }
                        break;
                    }

                    if (!loaded_file) {
                        log_error("Unresolved global symbol '%s'", sym->name);
                        log_ctx_pop();
                        return false;
                    }

                    break;
                }
            }

            log_ctx_pop();

            list_remove(&file->list_entry);
            list_insert_tail(&ctx->processed, &file->list_entry);
        }

    } while (loaded_file);

    if (!list_empty(&ctx->unprocessed)) {
        log_fatal("Unable to resolve all global symbols");
        return false;
    }

    // All symbols are loaded, we can release all archives
    list_for_each_entry_safe(file, &ctx->archives, struct archive_file, list_entry) {
        remove_archive_file(file);
    }

    return true;
}
