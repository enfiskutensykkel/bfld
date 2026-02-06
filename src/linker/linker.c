#include "target.h"
#include "objectfile_reader.h"
#include "archive_reader.h"
#include "logging.h"
#include "linker.h"
#include "stringpool.h"
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
#include "archives.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>


int log_level = 2;  // initial log level
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
    memset(&ctx->archives, 0, sizeof(struct archives));

    ctx->target_march = target;
    ctx->target_cpu_align = backend->cpu_code_alignment;
    ctx->target_pgsz_min = backend->min_page_size;
    ctx->target_pgsz_max = backend->max_page_size;
    ctx->target_sect_align = backend->section_boundary;
    ctx->target_is_be = backend->big_endian;

    ctx->base_addr = 0;
    ctx->entry_addr = 0;

    log_trace("Created linker context");
    return ctx;
}


void linker_put(struct linkerctx *ctx)
{
    assert(ctx != NULL);
    assert(ctx->refcnt > 0);

    if (--(ctx->refcnt) == 0) {
        struct section *sect;
        log_trace("Destroying linker context");

        while ((sect = sections_pop(&ctx->sections)) != NULL) {
            section_clear_relocs(sect);
            section_put(sect);
        }

        symbols_clear(&ctx->unresolved);
        globals_clear(&ctx->globals);

        sections_clear(&ctx->sections);
        archives_clear_symbols(&ctx->archives);

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

    if (reader == NULL) {
        reader = archive_reader_probe(archive->file_data, archive->file_size);
    }

    if (reader == NULL) {
        log_error("Unrecognized file format");
        log_ctx_pop();
        return false;
    }

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
    
    if (after - before == 0) {
        log_notice("Archive does not provide any additional symbols");
    } else {
        log_debug("Archive provides %llu new symbols", after - before);
    }

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

    uint32_t march = 0;

    if (reader == NULL) {
        reader = objectfile_reader_probe(objfile->file_data, objfile->file_size, &march);
    } else {
        reader->probe_file(objfile->file_data, objfile->file_size, &march);
    }
    if (reader == NULL) {
        log_error("Unrecognized file format");
        log_ctx_pop();
        return false;
    }

    if (march == 0) {
        log_warning("File has unknown machine code architecture");
    }

    if (ctx->target_march != 0 && march != ctx->target_march) {
        log_fatal("File's machine code architecture differs from target");
        log_ctx_pop();
        return false;
    }

    struct string_pool *strpool = string_pool_alloc();
    if (strpool == NULL) {
        log_ctx_pop();
        return false;
    }

    log_debug("Loading object file using front-end '%s'", reader->name);

    status = reader->parse_file(objfile->file_data, objfile->file_size,
                                objfile, strpool, &secttab, &symtab);
    while (log_ctx > current_log_ctx) {
        log_warning("Unwinding log context stack");
        log_ctx_pop();
    }

    if (status != 0) {
        log_error("Failed to load object file: %d", status);
        goto leave;
    }

    // Add file's global symbols to the symbol queue
    uint64_t defined = 0;
    uint64_t undefined = 0;
    for (uint64_t i = 0; symtab.nsymbols > 0 && i < symtab.capacity; ++i) {
        struct symbol *sym = symbol_table_at(&symtab, i);

        if (sym == NULL || sym->binding == SYMBOL_LOCAL) {
            symbol_table_remove(&symtab, i);
            continue;
        }

        struct symbol *existing = sym;

        if (symbol_is_defined(sym) || sym->is_common) {
            ++defined;
        }

        if (symbol_is_defined(sym)) {
            if (strncmp(".text._", section_name(sym->section), 7) == 0) {
                log_notice("Symbol '%s' is defined in section %s", symbol_name(sym), section_name(sym));
            }
        }
        
        status = globals_insert_symbol(&ctx->globals, sym, &existing);
        if (status != EEXIST && status != 0) {
            goto leave;
        }
        if (status == EEXIST) {
            // Symbol already exists in the global symbol table, merge them and keep the existing
            if (!symbol_merge(existing, sym)) {
                status = EINVAL;
                log_error("Failed to merge symbol definition for symbol '%s'", symbol_name(sym));
                goto leave;
            }
            symbol_undefine(sym);
        }

        if (!symbol_is_defined(existing)) {
            log_trace("Adding undefined symbol '%s' to unresolved queue", symbol_name(existing));

            if (!symbols_push(&ctx->unresolved, existing)) {
                goto leave;
            }

            ++undefined;
        }

        symbol_table_remove(&symtab, i);
    }
    log_debug("File defines %llu symbols and references %llu symbols", defined, undefined);

    // Add file's sections to the sections queue
    log_debug("File defines %llu sections", secttab.nsections);
    for (uint64_t i = 0; secttab.nsections > 0 && i < secttab.capacity; ++i) {
        struct section *sect = section_table_at(&secttab, i);
        
        if (sect == NULL) {
            continue;
        }

        // Add section to the section worklist
        if (!sections_push(&ctx->sections, sect)) {
            goto leave;
        }

        // Fixup relocations that point to global symbols
        list_for_each_entry(reloc, &sect->relocs, struct reloc, list_entry) {
            struct symbol *global = globals_find_symbol(&ctx->globals, symbol_name(reloc->symbol));

            if (global != NULL && global != reloc->symbol) {
                symbol_put(reloc->symbol);
                reloc->symbol = symbol_get(global);
            }
        }

        section_table_remove(&secttab, i);
    }

    success = true;

leave:
    for (uint64_t i = 0; secttab.nsections > 0 && i < secttab.capacity; ++i) {
        struct section *sect = section_table_at(&secttab, i);
        if (sect != NULL) {
            section_clear_relocs(sect);
            section_table_remove(&secttab, i);
        }
    }

    log_ctx_pop();
    string_pool_put(strpool);
    symbol_table_clear(&symtab);
    section_table_clear(&secttab);
    return success;
}


bool linker_resolve_globals(struct linkerctx *ctx)
{
    struct symbol *sym;

    while ((sym = symbols_pop(&ctx->unresolved)) != NULL) {

        if (symbol_is_defined(sym) || sym->is_common) {
            symbol_put(sym);
            continue;
        }

        // Try to find an archive that provides the undefined symbol
        struct archive_member *m = archives_find_symbol(&ctx->archives, symbol_name(sym));
        if (m == NULL) {
            symbol_put(sym);
            log_error("Undefined reference to symbol '%s'", symbol_name(sym));
            return false;
        }

        // Member provides the symbol, but it is already loaded 
        // We don't try to load it again
        if (archive_is_member_extracted(m)) {
            log_ctx_new(m->objfile->name);
            log_error("Object file is already loaded but symbol '%s' is still undefined", symbol_name(sym));
            log_ctx_pop();
            symbol_put(sym);
            return false;
        }

        log_trace("Symbol '%s' is provided by archive %s", symbol_name(sym), m->archive->name);

        struct objectfile *objfile = archive_extract_member(m);
        if (objfile != NULL) {
            if (!linker_load_objectfile(ctx, objfile, NULL)) {
                objectfile_put(objfile);
                symbol_put(sym);
                return false;
            }
            objectfile_put(objfile);
        }

        if (!symbol_is_defined(sym) && !sym->is_common) {
            log_error("Symbol '%s' was already provided by archive, but is still undefined",
                    symbol_name(sym));
            symbol_put(sym);
            return false;
        }

        symbol_put(sym);
    }

    // All symbols were resolved, we don't need to hold archives in memory any longer
    archives_clear_symbols(&ctx->archives);

    return true;
}
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
//        sect = sections_at(keep, i);
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
// TODO: linker_sweep, first go through symbols and nullify/undefine symbols that point to unused sections, then clear sections
