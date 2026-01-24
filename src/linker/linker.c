#include "backend.h"
#include "objfile_frontend.h"
#include "archive_frontend.h"
#include "logging.h"
#include "linker.h"
#include "objfile.h"
#include "utils/list.h"
#include "utils/align.h"
#include "sections.h"
#include "section.h"
#include "symbols.h"
#include "symbol.h"
#include "globals.h"
#include "mfile.h"
#include "archive.h"
#include "image.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>


int log_level = 1;  // initial log level
int log_ctx = 0;    // initial log context
log_ctx_t log_ctx_stack[LOG_CTX_MAX] = {0};


struct linkerctx * linker_create(const char *name)
{
    struct linkerctx *ctx = malloc(sizeof(struct linkerctx));
    if (ctx == NULL) {
        return NULL;
    }

    ctx->globals = globals_alloc();
    if (ctx->globals == NULL) {
        free(ctx);
        return NULL;
    }

    memset(&ctx->sections, 0, sizeof(struct sections));
    memset(&ctx->unresolved, 0, sizeof(struct symbols));

    if (name != NULL) {
        ctx->name = strdup(name);
    }

    ctx->target = 0;
    list_head_init(&ctx->archives);
    ctx->gc_sections = false;
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
        list_for_each_entry_safe(file, &ctx->archives, struct archive_file, list_entry) {
            remove_archive_file(file);
        }

        symbols_clear(&ctx->unresolved);
        sections_clear(&ctx->sections);

        globals_put(ctx->globals);

        if (ctx->name != NULL) {
            free(ctx->name);
        }
        free(ctx);
    }
}


bool linker_add_archive(struct linkerctx *ctx, struct archive *ar, 
                        const struct archive_frontend *fe)
{
    int current_log_ctx = log_ctx_new(ar->name);

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
    assert(log_ctx == current_log_ctx);
    while (log_ctx > current_log_ctx) {
        log_warning("Unwinding log context stack");
        log_ctx_pop();
    }

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
    int current_log_ctx = log_ctx_new(objfile->name);

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

    if (ctx->target != 0 && ctx->target != march) {
        log_fatal("Differing machine code targets is not supported");
        log_ctx_pop();
        return false;
    }
    ctx->target = march;

    const struct backend *be = backend_lookup(march);
    if (be == NULL) {
        log_error("Unsupported machine code architecture");
        log_ctx_pop();
        return false;
    }

    log_trace("Front-end '%s' is best match for object file", fe->name);

    struct section_table sections = {0};
    struct symbol_table symbols = {0};

    int status = fe->parse_file(objfile->file_data, objfile->file_size, 
                                objfile, &sections, &symbols);
    assert(log_ctx == current_log_ctx);
    while (log_ctx > current_log_ctx) {
        log_warning("Unwinding log context stack");
        log_ctx_pop();
    }

    if (status != 0) {
        log_error("Parsing file failed: %d", status);
        log_ctx_pop();
        symbol_table_clear(&symbols);
        section_table_clear(&sections);
        return false;
    }

    // Add file's symbols to the global symbol table
    log_debug("File references %llu symbols", symbols.nsymbols);
    for (uint64_t i = 0; symbols.nsymbols > 0 && i < symbols.capacity; ++i) {
        struct symbol *sym = symbol_table_at(&symbols, i);
        if (sym == NULL) {
            continue;
        }

        if (sym->binding == SYMBOL_LOCAL) {
            symbol_table_remove(&symbols, i);
            continue;
        }

        struct symbol *existing = sym;

        status = globals_insert_symbol(ctx->globals, sym, &existing);
        if (status == EEXIST) {
            // Symbol already exists in the global symbol table, merge them and keep the existing
            status = symbol_merge(existing, sym);
        }

        if (status != 0) {
            log_ctx_pop();
            symbol_table_clear(&symbols);
            section_table_clear(&sections);
            return false;
        }

        if (!symbol_is_defined(existing)) {
            symbols_push(&ctx->unresolved, existing);
        } 

        symbol_table_remove(&symbols, i);
    }

    // Add sections to the list of input sections
    log_debug("File has %llu sections", sections.nsections);
    for (uint64_t i = 0; sections.nsections > 0 && i < sections.capacity; ++i) {
        struct section *sect = section_table_at(&sections, i);
        if (sect == NULL) {
            continue;
        }

        // Add file's sections to the global section list
        sections_push(&ctx->sections, sect);

        // Fix relocations
        list_for_each_entry_safe(reloc, &sect->relocs, struct reloc, list_entry) {
            struct symbol *global = globals_find_symbol(ctx->globals, reloc->symbol->name);

            if (global != NULL && global != reloc->symbol) {
                symbol_put(reloc->symbol);
                reloc->symbol = symbol_get(global);
            }
        }

        section_table_remove(&sections, i);
    }

    log_trace("Added object file to input files");

    section_table_clear(&sections);
    symbol_table_clear(&symbols);
    log_ctx_pop();
    return true;
}


bool linker_resolve_globals(struct linkerctx *ctx)
{
    struct symbol *sym;

    while ((sym = symbols_pop(&ctx->unresolved)) != NULL) {

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
                log_error("Undefined reference to symbol '%s'", sym->name);
                symbol_put(sym);
                return false;
            }
        }

        symbol_put(sym);
    }

    return true;
}


bool linker_create_common_section(struct linkerctx *ctx)
{
    bool success = false;
    struct symbols buckets[16] = {0};  // supports up to 2^15 buckets (32 kB alignment)

    struct rb_node *node = rb_first(&ctx->globals->map);

    // Identify common symbols and sort them based on alignment
    while (node != NULL) {
        struct symbol *symbol = globals_symbol(node);
        node = rb_next(node);

        if (!symbol->is_common || (!symbol->is_used && ctx->gc_sections)) {
            continue;
        }

        uint64_t power = 0;
        uint64_t align = symbol->align > 0 ? symbol->align : 1;

        while (align >>= 1) {
            ++power;
        }

        if (power < 16) {
            symbols_push(&buckets[power], symbol);
        } else {
            log_warning("Symbol '%s' has a very high alignment requirement. Fall back to 32 kB.",
                    symbol->name);
            symbols_push(&buckets[15], symbol);
        }
    }

    struct section *common = section_alloc(NULL, ".common", SECTION_ZERO, NULL, 0);
    if (common == NULL) {
        goto leave;
    }

    // Calculate offsets and pack common symbols
    uint64_t offset = 0;
    uint64_t max_align = 0;
    for (int i = 15; i >= 0; --i) {
        struct symbols *syms = &buckets[i];
        struct symbol *sym;

        while ((sym = symbols_pop(syms)) != NULL) {
            offset = align_to(offset, sym->align);
            symbol_bind_definition(sym, common, offset, sym->size);
            offset += sym->size;
            max_align = sym->align > max_align ? sym->align : max_align;
        }
    }
    common->size = offset;
    common->align = max_align;

    if (offset > 0) {
        log_debug("Created artificial section %s", common->name);
        common->is_alive = true;
        sections_push(&ctx->sections, common);
    }
    section_put(common);
    success = true;

leave:
    for (int i = 0; i < 16; ++i) {
        symbols_clear(&buckets[i]);
    }
    return success;
}


void linker_gc_sections(struct linkerctx *ctx, const struct sections *keep)
{
    struct sections wl = {0};
    struct section *sect;

    ctx->gc_sections = true;

    sections_reserve(&wl, ctx->sections.q.capacity);

    // Start with using all sections marked as kept
    for (uint64_t i = 0; i < keep->q.capacity; ++i) {
        sect = sections_peek(keep, i);
        if (sect != NULL) {
            sect->is_alive = true;
            sections_push(&wl, sect);
        }
    }

    // Mark all "alive" sections
    while ((sect = sections_pop(&wl)) != NULL) {
        assert(sect->is_alive);
    
        // Follow relocations and mark target sections as alive
        list_for_each_entry(r, &sect->relocs, struct reloc, list_entry) {
            struct symbol *sym = r->symbol;
            assert(sym != NULL);

            if (!sym->is_used) {
                sym->is_used = true;
                log_debug("Marking symbol '%s' as alive", sym->name);

                if (symbol_is_defined(sym) && sym->section != NULL) {
                    struct section *target = sym->section;

                    if (!target->is_alive) {
                        sections_push(&wl, target);
                        target->is_alive = true;
                        log_debug("Marking section %s as alive", target->name);
                    }
                }
            }
        }

        section_put(sect);
    }

    sections_clear(&wl);
}

