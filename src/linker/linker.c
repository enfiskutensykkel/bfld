#include "target.h"
#include "objectfile_reader.h"
#include "archive_reader.h"
#include "logging.h"
#include "linker.h"
#include "objectfile.h"
#include "utils/list.h"
#include "utils/align.h"
#include "sections.h"
#include "section.h"
#include "symbols.h"
#include "symbol.h"
#include "globals.h"
#include "mfile.h"
#include "archive.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>


int log_level = 1;  // initial log level
int log_ctx = 0;    // initial log context
log_ctx_t log_ctx_stack[LOG_CTX_MAX] = {0};


struct linkerctx * linker_alloc(const char *name, uint32_t target)
{
    const struct target *backend = target_lookup(target);
    if (backend == NULL) {
        log_fatal("Unsupported machine code architecture");
        return NULL;
    }

    struct linkerctx *ctx = malloc(sizeof(struct linkerctx));
    if (ctx == NULL) {
        return NULL;
    }

    ctx->name = malloc(strlen(name) + 1);
    if (ctx->name == NULL) {
        free(ctx);
        return NULL;
    }
    strcpy(ctx->name, name);

    ctx->refcnt = 1;
    ctx->gc_sections = false;
    memset(&ctx->globals, 0, sizeof(struct globals));
    memset(&ctx->sections, 0, sizeof(struct sections));
    memset(&ctx->unresolved, 0, sizeof(struct symbols));
    memset(&ctx->archives, 0, sizeof(struct archive_index));

    ctx->target_march = target;
    ctx->target_cpu_align = backend->cpu_code_alignment;
    ctx->target_pgsz_min = backend->min_page_size;
    ctx->target_pgsz_max = backend->max_page_size;
    ctx->target_sect_align = backend->section_boundary;
    ctx->target_is_be = backend->big_endian;

    ctx->base_addr = 0;
    ctx->entry_addr = 0;

    log_debug("Created linker context");
    return ctx;
}


void linker_put(struct linkerctx *ctx)
{
    assert(ctx != NULL);
    assert(ctx->refcnt > 0);

    if (--(ctx->refcnt) == 0) {
        log_debug("Destroyed linker context");
        
        sections_clear(&ctx->sections);
        symbols_clear(&ctx->unresolved);
        globals_clear(&ctx->globals);
        archive_index_clear(&ctx->archives);

        if (ctx->name != NULL) {
            free(ctx->name);
        }
        free(ctx);
    }
}


struct linkerctx * linker_get(struct linkerctx *ctx)
{
    assert(ctx != NULL);
    assert(ctx->refcnt > 0);
    ctx->refcnt++;
    return ctx;
}


bool linker_read_archive(struct linkerctx *ctx, 
                         struct archive *archive,
                         const struct archive_reader *reader)
{
    int current_log_ctx = log_ctx_new(archive->name);

    // FIXME: handle reader==NULL

    log_debug("Reading archive using reader '%s'", reader->name);

    uint64_t before = ctx->archives.entries;
    int status = reader->parse_file(archive->file_data, archive->file_size, 
                                    archive, &ctx->archives);
    while (log_ctx > current_log_ctx) {
        log_warning("Unwinding log context stack");
        log_ctx_pop();
    }

    if (status != 0) {
        log_error("Failed to read archive file: %d", status);
        log_ctx_pop();
        return false;
    }
    
    uint64_t after = ctx->archives.entries;
    log_debug("Archive has %zu members and provides %llu new symbols", 
            archive->nmembers, after - before);

    log_debug("Parsed archive file");
    log_ctx_pop();
    return true;
}


bool linker_load_objectfile(struct linkerctx *ctx,
                            struct objectfile *objfile,
                            const struct objectfile_reader *reader)
{
    int status = 0;
    bool success = false;
    int current_log_ctx = log_ctx_new(objfile->name);

    struct section_table secttab = {0};
    struct symbol_table symtab = {0};

    log_debug("Loading object file using front-end '%s'", reader->name);

    status = reader->parse_file(objfile->file_data, objfile->file_size,
                                objfile, &secttab, &symtab);
    while (log_ctx > current_log_ctx) {
        log_warning("Unwinding log context stack");
        log_ctx_pop();
    }

    if (status != 0) {
        log_error("Failed to load object file: %d", status);
        goto leave;
    }

    // Add file's global symbols to the symbol queue
    log_debug("File references %llu symbols", symtab.nsymbols);
    for (uint64_t i = 0; symtab.nsymbols > 0 && i < symtab.capacity; ++i) {
        struct symbol *sym = symbol_table_at(&symtab, i);

        if (sym != NULL && sym->binding != SYMBOL_LOCAL) {
            if (!symbols_push(&ctx->unresolved, sym)) {
                goto leave;
            }
        }

        // FIXME: look up in globals and resolve here

        symbol_table_remove(&symtab, i);
    }

    // Add file's sections to the sections queue
    log_debug("File defines %llu sections", secttab.nsections);
    for (uint64_t i = 0; secttab.nsections > 0 && i < secttab.capacity; ++i) {
        struct section *sect = section_table_at(&secttab, i);

        if (sect != NULL) {
            if (!sections_push(&ctx->sections, sect)) {
                goto leave;
            }
        }

        // FIXME: fixup relocations here

        section_table_remove(&secttab, i);
    }

    log_trace("Successfully loaded object file");
    success = true;

leave:
    log_ctx_pop();
    section_table_clear(&secttab);
    symbol_table_clear(&symtab);
    return success;
}




//bool linker_add_archive(struct linkerctx *ctx, struct archive *ar, 
//                        const struct archive_reader *fe)
//{
//    int current_log_ctx = log_ctx_new(ar->name);
//
//    log_debug("Reading archive");
//
//    if (fe == NULL) {
//        fe = archive_reader_probe(ar->file_data, ar->file_size);
//    }
//
//    if (fe == NULL) {
//        log_error("Unrecognized file format");
//        log_ctx_pop();
//        return false;
//    }
//    log_trace("Front-end '%s' is best match for archive", fe->name);
//
//    int status = fe->parse_file(ar->file_data, ar->file_size, ar);
//    assert(log_ctx == current_log_ctx);
//    while (log_ctx > current_log_ctx) {
//        log_warning("Unwinding log context stack");
//        log_ctx_pop();
//    }
//
//    if (status != 0) {
//        log_ctx_pop();
//        return false;
//    }
//
//    struct archive_file *arfile = malloc(sizeof(struct archive_file));
//    if (arfile == NULL) {
//        log_ctx_pop();
//        return false;
//    }
//
//    arfile->ctx = ctx;
//    list_insert_tail(&ctx->archives, &arfile->list_entry);
//    arfile->archive = archive_get(ar);
//    
//    log_trace("Archive file added");
//
//    log_ctx_pop();
//    return true;
//}


//bool linker_add_input_file(struct linkerctx *ctx, struct objfile *objfile,
//                           const struct objfile_reader *reader)
//{
//    uint32_t march = 0;
//    int current_log_ctx = log_ctx_new(objfile->name);
//
//    log_debug("Reading object file");
//
//    if (reader == NULL) {
//        reader = objfile_reader_probe(objfile->file_data, objfile->file_size, &march);
//    } else {
//        reader->probe_file(objfile->file_data, objfile->file_size, &march);
//    }
//
//    if (reader == NULL) {
//        log_error("Unrecognized file format");
//        log_ctx_pop();
//        return false;
//    }
//
//    if (march == 0) {
//        log_error("Unknown machine code architecture");
//        log_ctx_pop();
//        return false;
//    }
//
//    if (ctx->target != 0 && ctx->target != march) {
//        log_fatal("Differing machine code targets is not supported");
//        log_ctx_pop();
//        return false;
//    }
//    ctx->target = march;
//
//    const struct target *t = target_lookup(march);
//    if (t == NULL) {
//        log_error("Unsupported machine code architecture");
//        log_ctx_pop();
//        return false;
//    }
//
//    log_trace("Front-end '%s' is best match for object file", reader->name);
//
//    struct section_table sections = {0};
//    struct symbol_table symbols = {0};
//
//    int status = reader->parse_file(objfile->file_data, objfile->file_size, 
//                                    objfile, &sections, &symbols);
//    assert(log_ctx == current_log_ctx);
//    while (log_ctx > current_log_ctx) {
//        log_warning("Unwinding log context stack");
//        log_ctx_pop();
//    }
//
//    if (status != 0) {
//        log_error("Parsing file failed: %d", status);
//        log_ctx_pop();
//        symbol_table_clear(&symbols);
//        section_table_clear(&sections);
//        return false;
//    }
//
//    // Add file's symbols to the global symbol table
//    log_debug("File references %llu symbols", symbols.nsymbols);
//    for (uint64_t i = 0; symbols.nsymbols > 0 && i < symbols.capacity; ++i) {
//        struct symbol *sym = symbol_table_at(&symbols, i);
//
//        if (sym == NULL || sym->binding == SYMBOL_LOCAL) {
//            symbol_table_remove(&symbols, i);
//            continue;
//        }
//
//        struct symbol *existing = sym;
//
//        status = globals_insert_symbol(ctx->globals, sym, &existing);
//        if (status == EEXIST) {
//            // Symbol already exists in the global symbol table, merge them and keep the existing
//            status = symbol_merge(existing, sym);
//        }
//
//        if (status != 0) {
//            log_ctx_pop();
//            symbol_table_clear(&symbols);
//            section_table_clear(&sections);
//            return false;
//        }
//
//        if (!symbol_is_defined(existing)) {
//            symbols_push(&ctx->unresolved, existing);
//        } 
//
//        symbol_table_remove(&symbols, i);
//    }
//
//    // Add sections to the list of input sections
//    log_debug("File has %llu sections", sections.nsections);
//    for (uint64_t i = 0; sections.nsections > 0 && i < sections.capacity; ++i) {
//        struct section *sect = section_table_at(&sections, i);
//        if (sect == NULL) {
//            continue;
//        }
//
//        // Add file's sections to the global section list
//        sections_push(&ctx->sections, sect);
//
//        // Fix relocations
//        list_for_each_entry_safe(reloc, &sect->relocs, struct reloc, list_entry) {
//            struct symbol *global = globals_find_symbol(ctx->globals, reloc->symbol->name);
//
//            if (global != NULL && global != reloc->symbol) {
//                symbol_put(reloc->symbol);
//                reloc->symbol = symbol_get(global);
//            }
//        }
//
//        section_table_remove(&sections, i);
//    }
//
//    section_table_clear(&sections);
//    symbol_table_clear(&symbols);
//    log_ctx_pop();
//    return true;
//}


//bool linker_resolve_globals(struct linkerctx *ctx)
//{
//    struct symbol *sym;
//
//    while ((sym = symbols_pop(&ctx->unresolved)) != NULL) {
//
//        if (!symbol_is_defined(sym) && !sym->is_common) {
//            log_trace("Attempting to resolve symbol '%s'", sym->name);
//
//            // Try to find an archive that provides the undefined symbol
//            list_for_each_entry(ar, &ctx->archives, struct archive_file, list_entry) {
//                struct archive_member *m = archive_find_symbol(ar->archive, sym->name);
//
//                // No member provides the symbol
//                if (m == NULL) {
//                    continue;
//                }
//
//                // Member provides the symbol, but it is already loaded 
//                // We don't try to load it again
//                if (m->objfile != NULL) {
//                    log_warning("Symbol '%s' was provided by archive %s, but is still undefined",
//                            sym->name, m->archive->name);
//                    continue;
//                }
//
//                log_debug("Found symbol '%s' in archive %s", sym->name, m->archive->name);
//                struct objfile *obj = archive_get_objfile(m);
//                if (obj != NULL) {
//                    if (!linker_add_input_file(ctx, obj, NULL)) {
//                        objfile_put(obj);
//                        symbol_put(sym);
//                        return false;
//                    }
//                    objfile_put(obj);
//                }
//                break;
//            }
//
//            if (!symbol_is_defined(sym) && !sym->is_common) {
//                log_error("Symbol '%s' is undefined", sym->name);
//                symbol_put(sym);
//                return false;
//            }
//        }
//
//        symbol_put(sym);
//    }
//
//    return true;
//}
//
//
//bool linker_create_common_section(struct linkerctx *ctx)
//{
//    bool success = false;
//    struct symbols buckets[16] = {0};  // supports up to 2^15 buckets (32 kB alignment)
//
//    struct rb_node *node = rb_first(&ctx->globals->map);
//
//    // Identify common symbols and sort them based on alignment
//    while (node != NULL) {
//        struct symbol *symbol = globals_symbol(node);
//        node = rb_next(node);
//
//        if (!symbol->is_common || (!symbol->is_used && ctx->gc_sections)) {
//            continue;
//        }
//
//        uint64_t power = align_log2(symbol->align);
//        if (power < 16) {
//            symbols_push(&buckets[power], symbol);
//        } else {
//            log_warning("Symbol '%s' has a very high alignment requirement. Fall back to 32 kB.",
//                    symbol->name);
//            symbols_push(&buckets[15], symbol);
//        }
//    }
//
//    struct section *common = section_alloc(NULL, ".common", SECTION_ZERO, NULL, 0);
//    if (common == NULL) {
//        goto leave;
//    }
//
//    // Calculate offsets and pack common symbols
//    uint64_t offset = 0;
//    uint64_t max_align = 0;
//    for (int i = 15; i >= 0; --i) {
//        struct symbols *syms = &buckets[i];
//        struct symbol *sym;
//
//        while ((sym = symbols_pop(syms)) != NULL) {
//            offset = align_to(offset, sym->align);
//            symbol_bind_definition(sym, common, offset, sym->size);
//            offset += sym->size;
//            max_align = sym->align > max_align ? sym->align : max_align;
//            symbol_put(sym);
//        }
//    }
//    common->size = offset;
//    common->align = max_align;
//
//    if (offset > 0) {
//        log_debug("Created artificial section %s", common->name);
//        common->is_alive = ctx->gc_sections;  // mark as alive if DCE was already performed
//        sections_push(&ctx->sections, common);
//    }
//    section_put(common);
//    success = true;
//
//leave:
//    for (int i = 0; i < 16; ++i) {
//        symbols_clear(&buckets[i]);
//    }
//    return success;
//}
//
//
//void linker_gc_sections(struct linkerctx *ctx, const struct sections *keep)
//{
//    struct sections wl = {0};
//    struct section *sect;
//
//    ctx->gc_sections = true;
//
//    sections_reserve(&wl, ctx->sections.q.capacity);
//
//    // Start with using all sections marked as kept
//    for (uint64_t i = 0; i < keep->q.capacity; ++i) {
//        sect = sections_peek(keep, i);
//        if (sect != NULL) {
//            sect->is_alive = true;
//            sections_push(&wl, sect);
//        }
//    }
//
//    // Mark all "alive" sections
//    while ((sect = sections_pop(&wl)) != NULL) {
//        assert(sect->is_alive);
//    
//        // Follow relocations and mark target sections as alive
//        list_for_each_entry(r, &sect->relocs, struct reloc, list_entry) {
//            struct symbol *sym = r->symbol;
//            assert(sym != NULL);
//
//            if (!sym->is_used) {
//                sym->is_used = true;
//                log_debug("Marking symbol '%s' as alive", sym->name);
//
//                if (symbol_is_defined(sym) && sym->section != NULL) {
//                    struct section *target = sym->section;
//
//                    if (!target->is_alive) {
//                        sections_push(&wl, target);
//                        target->is_alive = true;
//                        log_debug("Marking section %s as alive", target->name);
//                    }
//                }
//            }
//        }
//
//        section_put(sect);
//    }
//
//    sections_clear(&wl);
//}

