#include "backend.h"
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
#include <errno.h>


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


/*
 * Linker machine code architecture back-end.
 */
struct be_entry
{
    struct list_head node;
    const struct backend *backend;
    uint32_t march;
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


/*
 * List of back-ends.
 */
static struct list_head backends = LIST_HEAD_INIT(backends);


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


void backend_register(const struct backend *be, uint32_t march)
{
    if (be == NULL || be->name == NULL || march == 0) {
        return;
    }

    if (be->apply_reloc == NULL) {
        return;
    }

    if (backend_lookup(march) != NULL) {
        // already registered
        return;
    }

    struct be_entry *entry = malloc(sizeof(struct be_entry));
    if (entry == NULL) {
        return;
    }

    entry->backend = be;
    entry->march = march;
    list_insert_tail(&backends, &entry->node);
}


/*
 * Remove all registered front-ends and back-ends.
 */
__attribute__((destructor(65535)))
static void remove_registered(void)
{
    list_for_each_entry_safe(entry, &objfile_frontends, struct objfile_fe_entry, node) {
        list_remove(&entry->node);
        free(entry);
    }

    list_for_each_entry_safe(entry, &archive_frontends, struct archive_fe_entry, node) {
        list_remove(&entry->node);
        free(entry);
    }

    list_for_each_entry_safe(entry, &backends, struct be_entry, node) {
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


const struct backend * backend_lookup(uint32_t march) 
{
    list_for_each_entry(entry, &backends, struct be_entry, node) {
        if (entry->march == march) {
            return entry->backend;
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

    ctx->globals = globals_alloc("globals");
    if (ctx->globals == NULL) {
        free(ctx);
        log_ctx_pop();
        return NULL;
    }

    ctx->sections = sections_alloc("sections");
    if (ctx->sections == NULL) {
        globals_put(ctx->globals);
        free(ctx);
        log_ctx_pop();
        return NULL;
    }

    ctx->unresolved = symbols_alloc("unresolved");
    if (ctx->unresolved == NULL) {
        sections_put(ctx->sections);
        globals_put(ctx->globals);
        free(ctx);
        log_ctx_pop();
        return NULL;
    }

    if (name != NULL) {
        ctx->name = strdup(name);
    }

    ctx->march = 0;
    ctx->log_ctx = log_ctx;
    list_head_init(&ctx->archives);
    return ctx;
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

        list_for_each_entry_safe(file, &ctx->archives, struct archive_file, list_entry) {
            remove_archive_file(file);
        }

        symbols_put(ctx->unresolved);
        globals_put(ctx->globals);
        sections_put(ctx->sections);

        if (ctx->name != NULL) {
            free(ctx->name);
        }
        free(ctx);
        log_ctx_pop();
    }
}


bool linker_add_archive(struct linkerctx *ctx, struct archive *ar, 
                        const struct archive_frontend *fe)
{
    log_ctx_new(ar->name);

    if (fe == NULL) {
        fe = archive_frontend_probe(ar->file_data, ar->file_size);
    }

    if (fe == NULL) {
        log_error("Unrecognized file format");
        log_ctx_pop();
        return false;
    }
    log_trace("Front-end '%s' is best match for archive", fe->name);

    int status = fe->parse_file(ar->file_data, ar->file_size, ar);
    if (status != 0) {
        log_ctx_pop();
        return false;
    }

    struct archive_file *arfile = malloc(sizeof(struct archive_file));
    if (arfile == NULL) {
        log_ctx_pop();
        return false;
    }

    arfile->ctx = ctx;
    list_insert_tail(&ctx->archives, &arfile->list_entry);
    arfile->archive = archive_get(ar);
    
    log_trace("Archive file added");
    log_ctx_pop();
    return true;
}


bool linker_add_input_file(struct linkerctx *ctx, struct objfile *objfile,
                           const struct objfile_frontend *fe)
{
    uint32_t march = 0;
    log_ctx_new(objfile->name);

    if (fe == NULL) {
        fe = objfile_frontend_probe(objfile->file_data, objfile->file_size, &march);
    } else {
        fe->probe_file(objfile->file_data, objfile->file_size, &march);
    }

    if (fe == NULL) {
        log_error("Unrecognized file format");
        log_ctx_pop();
        return false;
    }

    if (march == 0) {
        log_error("Unknown machine code architecture");
        log_ctx_pop();
        return false;
    }

    if (ctx->march != 0 && ctx->march != march) {
        log_fatal("Mixing machine code architecture is not supported");
        log_ctx_pop();
        return false;
    }
    ctx->march = march;

    const struct backend *be = backend_lookup(march);
    if (be == NULL) {
        log_error("Unsupported machine code architecture");
        log_ctx_pop();
        return false;
    }

    log_trace("Front-end '%s' is best match for object file", fe->name);

    struct symbols *symbols = symbols_alloc(objfile->name);
    if (symbols == NULL) {
        log_ctx_pop();
        return false;
    }

    struct sections *sections = sections_alloc(objfile->name);
    if (sections == NULL) {
        symbols_put(symbols);
        log_ctx_pop();
        return false;
    }

    int status = fe->parse_file(objfile->file_data, objfile->file_size, 
                                objfile, sections, symbols);
    if (status != 0) {
        symbols_put(symbols);
        sections_put(sections);
        log_ctx_pop();
        return false;
    }

    // Add file's sections to the global section list
    for (uint64_t i = 0, n = 0; i < sections->capacity && n < sections->nsections; ++i) {
        struct section *sect = sections_at(sections, i);
        if (sect != NULL) {
            sections_push(ctx->sections, sect);
            ++n;
        }
    }

    // Add file's symbols to the global symbol table
    for (uint64_t i = 0, n = 0; i < symbols->capacity && n < symbols->nsymbols; ++i) {
        struct symbol *sym = symbols_at(symbols, i);
        if (sym == NULL) {
            continue;
        }

        ++n;

        if (sym->binding == SYMBOL_LOCAL) {
            continue;
        }

        struct symbol *existing = sym;

        status = globals_insert_symbol(ctx->globals, sym, &existing);
        if (status == EEXIST) {
            // Symbol already exists in the global symbol table, merge them and keep the existing
            status = symbol_merge(existing, sym);
        }

        if (status != 0) {
            symbols_put(symbols);
            sections_put(sections);
            log_ctx_pop();
            return false;
        }

        if (!symbol_is_defined(existing)) {
            symbols_push(ctx->unresolved, existing);
        } 
    }

    log_trace("Added object file to input files");

    symbols_put(symbols);
    sections_put(sections);
    log_ctx_pop();
    return true;
}


bool linker_resolve_globals(struct linkerctx *ctx)
{
    while (ctx->unresolved->nsymbols > 0) {
        struct symbol *sym = symbols_pop(ctx->unresolved);

        if (!symbol_is_defined(sym) && !sym->is_common) {
            log_debug("Symbol '%s' is undefined", sym->name);

            // Try to find an archive that provides the undefined symbol
            list_for_each_entry(ar, &ctx->archives, struct archive_file, list_entry) {
                struct archive_member *m = archive_find_symbol(ar->archive, sym->name);

                // No member provides the symbol
                if (m == NULL) {
                    continue;
                }

                // Member provides the symbol, but it is already loaded 
                // We don't try to load it again
                if (m->objfile != NULL) {
                    log_warning("Symbol '%s' was provided by archive %s, but is still undefined",
                            sym->name, m->archive->name);
                    continue;
                }

                log_debug("Found symbol '%s' in archive %s", sym->name, m->archive->name);
                struct objfile *obj = archive_get_objfile(m);
                if (obj != NULL) {
                    if (!linker_add_input_file(ctx, obj, NULL)) {
                        objfile_put(obj);
                        symbol_put(sym);
                        return false;
                    }
                    objfile_put(obj);
                }
                break;
            }

            if (!symbol_is_defined(sym)) {
                log_error("Unresolved global symbol '%s'", sym->name);
                symbol_put(sym);
                return false;
            }
        }

        symbol_put(sym);
    }

    // All symbols are loaded, we can release all archives
    list_for_each_entry_safe(file, &ctx->archives, struct archive_file, list_entry) {
        remove_archive_file(file);
    }

    return true;
}


void linker_gc_sections(struct linkerctx *ctx)
{
    struct sections *wl = sections_alloc("worklist");
    if (wl == NULL) {
        return;
    }

    // TODO: Add all entry points to wl

    // Mark all "alive" sections
    while (wl->nsections > 0) {
        assert(wl->sections[wl->maxidx] != NULL);

        struct section *sect = sections_pop(wl);
        assert(sect->is_alive);
    
        // Follow relocations and mark target sections as alive
        list_for_each_entry(r, &sect->relocs, struct reloc, list_entry) {
            struct symbol *sym = r->symbol;
            assert(sym != NULL);

            if (symbol_is_defined(sym) && sym->section != NULL) {
                struct section *target = sym->section;

                if (!target->is_alive) {
                    sections_push(wl, target);
                    target->is_alive = true;
                }
            }
        }

        section_put(sect);
    }

    //ctx->sections

    sections_put(wl);
}
