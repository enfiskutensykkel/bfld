#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <logging.h>
#include <mfile.h>
#include <linker.h>
#include <string.h>
#include <assert.h>
#include <symbols.h>
#include <globals.h>
#include <section.h>
#include <symbol.h>
#include <objectfile.h>
#include <archive.h>
#include <objectfile_reader.h>
#include <archive_reader.h>
#include "commandline.h"
#include <utils/list.h>

#define TARGET_X86_64 62



//static void print_symbols(FILE *fp, const struct image *img)
//{
//    static const char *typemap[] = {
//        [SYMBOL_NOTYPE] = "notype",
//        [SYMBOL_OBJECT] = "object",
//        [SYMBOL_TLS] = "tls",
//        [SYMBOL_SECTION] = "sect",
//        [SYMBOL_FUNCTION] = "func"
//    };
//
//    static const char *bindmap[] = {
//        [SYMBOL_WEAK] = "weak",
//        [SYMBOL_GLOBAL] = "global",
//        [SYMBOL_LOCAL] = "local"
//    };
//
//    fprintf(fp, "Symbol table for image '%s'\n", img->name);
//    fprintf(fp, "%-16s %8s %8s %8s %8s %-6s %-6s %-20s %-32s\n",
//            "VMA", "MemSize", "MemAlign", "FileOffs", "FileSize", "Type", "Bind", "Section", "Symbol");
//
//    for (uint64_t idx = 0; idx < img->symbols.nsymbols; ++idx) {
//        const struct symbol *sym = symbols_peek(&img->symbols, idx);
//
//        uint64_t value = image_get_symbol_address(img, sym);
//
//        const char *defname = "UNKNOWN";
//        size_t file_offset = 0;
//        size_t file_size = 0;
//
//        if (!symbol_is_defined(sym)) {
//            defname = "UNDEFINED";
//            
//        } else if (sym->is_absolute) {
//            defname = "ABSOLUTE";
//        } else {
//            struct output_section *sect = image_get_output_section(img, sym->section);
//            defname = sect->name;
//            file_offset = sect->file_offset + (value - sect->vaddr);
//            file_size = sect->type == SECTION_ZERO ? 0 : sym->size;
//        }
//
//        fprintf(fp, "%016lx ", value);
//        fprintf(fp, "%8lu ", sym->size);
//        fprintf(fp, "%8lu ", sym->align);
//        fprintf(fp, "%8zu ", file_offset);
//        fprintf(fp, "%8zu ", file_size);
//        fprintf(fp, "%-6s ", typemap[sym->type]);
//        fprintf(fp, "%-6s ", bindmap[sym->binding]);
//        fprintf(fp, "%-20.20s ", defname);
//        fprintf(fp, "%-32.32s", sym->name);
//        fprintf(fp, "\n");
//    }
//    fprintf(fp, "\n");
//}
//
//
//static void print_layout(FILE *fp, const struct image *img)
//{
//    fprintf(fp, "Layout for image '%s'\n", img->name);
//    fprintf(fp, "Base address: 0x%016lx\n", img->base_addr);
//    fprintf(fp, "Entry point : 0x%016lx\n", img->entry_addr);
//    fprintf(fp, "Memory size : %lu\n", img->size);
//    fprintf(fp, "File size   : %zu\n", img->file_size);
//
//    fprintf(fp, "\n%-16s %8s %8s %8s %8s %-20s\n", "VMA", "MemSize", "MemAlign", "FileOffs", "FileSize", "Section");
//    list_for_each_entry(sect, &img->sections, struct output_section, list_entry) {
//        fprintf(fp, "%016lx ", sect->vaddr);
//        fprintf(fp, "%8lu ", sect->size);
//        fprintf(fp, "%8lu ", sect->align);
//        fprintf(fp, "%8zu ", sect->file_offset);
//        fprintf(fp, "%8zu ", sect->file_size);
//        fprintf(fp, "%-20.20s", sect->name);
//        fprintf(fp, "\n");
//    }
//    fprintf(fp, "\n");
//}


static bool load_file(struct linkerctx *ctx, const char *pathname)
{
    struct mfile *file = NULL;

    log_ctx_new(pathname);

    int status = mfile_open_read(&file, pathname);
    if (status != 0) {
        log_ctx_pop();
        return false;
    }

    // Try to open as archive file
    const struct archive_reader *reader = archive_reader_probe(file->data, file->size);
    if (reader != NULL) {
        struct archive *ar = archive_alloc(file, file->name, file->data, file->size);

        if (ar != NULL) {
            bool success = linker_read_archive(ctx, ar, reader);
            archive_put(ar);
            mfile_put(file);
            log_ctx_pop();
            return success;
        }
    }

    // Try to open as object file
    const struct objectfile_reader *frontend = objectfile_reader_probe(file->data, file->size, NULL);

    if (frontend != NULL) {
        struct objectfile *obj = objectfile_alloc(file, file->name, file->data, file->size);

        if (obj != NULL) {
            bool success = linker_load_objectfile(ctx, obj, frontend);
            objectfile_put(obj);
            mfile_put(file);
            log_ctx_pop();
            return success;
        }
    }

    log_error("Unrecognized file format for file '%s'", pathname);
    mfile_put(file);
    log_ctx_pop();
    return false;
}


//static void linker_merge_sections(struct linkerctx *ctx)
//{
//    struct merge *merged[SECTION_MAX_TYPES] = {0};
//
//    for (uint32_t i = 0; i < SECTION_MAX_TYPES; ++i) {
//        uint32_t type = SECTION_MAX_TYPES - 1 - i;
//        const char *name = section_type_to_string(type);
//        merged[i] = merge_alloc(name, type);
//    }
//
//    for (uint64_t i = 0; i < ctx->sections.nsections; ++i) {
//        struct section *sect = sections_peek(&ctx->sections, i);
//
//        if (ctx->gc_sections && !sect->is_alive) {
//            continue;
//        }
//
//        struct merge *m = merged[sect->type];
//        printf("Adding %s to %s\n", sect->name, m->name);
//        merge_add_section(m, sect);
//    }
//
//leave:
//    for (uint32_t i = 0; i < SECTION_MAX_TYPES; ++i) {
//        if (merged[i] != NULL) {
//            merge_put(merged[i]);
//            merged[i] = NULL;
//        }
//    }
//}


//static struct image * linker_create_image(const char *name,
//                                          struct linkerctx *ctx,
//                                          uint64_t baseaddr)
//{
//    struct image *img = image_alloc(name, ctx->target);
//    if (img == NULL) {
//        log_fatal("Unable to create output image");
//        return NULL;
//    }
//
//    for (uint32_t i = 0; i < SECTION_MAX_TYPES; ++i) {
//        uint32_t type = SECTION_MAX_TYPES - 1 - i;
//        const char *name = section_type_to_string(type);
//        if (image_create_output_section(img, type, name) == NULL) {
//            log_fatal("Unable to create output image");
//            image_put(img);
//            return NULL;
//        }
//    }
//
//    // pop sections instead?
//    for (uint64_t i = 0; i < ctx->sections.nsections; ++i) {
//        struct section *sect = sections_peek(&ctx->sections, i);
//        const char *name = section_type_to_string(sect->type);
//
//        if (ctx->gc_sections && !sect->is_alive) {
//            continue;
//        }
//
//        struct output_section *outsect = image_find_output_section(img, name);
//
//        if (!image_add_section(outsect, sect)) {
//            image_put(img);
//            return NULL;
//        }
//    }
//
//    struct rb_node *node = rb_first(&ctx->globals->map);
//
//    while (node != NULL) {
//        struct symbol *sym = globals_symbol(node);
//        node = rb_next(node);
//
//        if (ctx->gc_sections && !symbol_is_alive(sym)) {
//            // remove symbol?
//            continue;
//        }
//
//        image_add_symbol(img, sym);
//    }
//
//    image_layout(img, baseaddr);
//    return img;
//}


int main(int argc, char **argv)
{
    struct bfld_options opts = {0};
    opts.output = "a.out";
    opts.entry = "_start";
    opts.gc_sections = true;

    int start = parse_args(argc, argv, &opts);
    if (start <= 0) {
        exit(-start);
    }

    struct linkerctx *ctx = linker_alloc(opts.output, TARGET_X86_64);
    if (ctx == NULL) {
        exit(2);
    }

    if (start >= argc) {
        log_error("No input files");
        linker_put(ctx);
        exit(1);
    }

    linker_add_got_section(ctx);

    for (int i = start; i < argc; ++i) {
        bool success = load_file(ctx, argv[i]);
        if (!success) {
            linker_put(ctx);
            exit(1);
        }
    } 

    if (sections_empty(&ctx->sections)) {
        log_error("No input files");
        linker_put(ctx);
        exit(1);
    }

    if (!linker_resolve_globals(ctx)) {
        linker_put(ctx);
        exit(2);
    }

    linker_put(ctx);
    exit(0);
}

//    // Resolve all unresolved symbols
//    if (!linker_resolve_globals(ctx)) {
//        linker_put(ctx);
//        close_archives();
//        exit(3);
//    }
//
//    // Identify sections that we need to keep
//    struct symbol *ep = globals_find_symbol(ctx->globals, entry);
//    if (ep == NULL) {
//        log_fatal("Undefined reference to '%s'", entry);
//        linker_put(ctx);
//        exit(3);
//    }
//    log_debug("Entry point: '%s'", ep->name);
//
//    if (gc_sections) {
//        struct sections keep = {0};
//        keep_symbol(&keep, ep);
//        linker_gc_sections(ctx, &keep);
//        sections_clear(&keep);
//    }
//
//    if (!linker_create_common_section(ctx)) {
//        linker_put(ctx);
//        exit(2);
//    }
//
////    struct image *img = linker_create_image(output_file, ctx, 0x400000);
////    if (img == NULL) {
////        linker_put(ctx);
////        exit(2);
////    }
//
////    img->entry_addr = image_get_symbol_address(img, ep);
//    linker_put(ctx);
//
////    if (show_symbols) {
////        print_symbols(stdout, img);
////    }
////
////    if (show_layout) {
////        print_layout(stdout, img);
////    }
//
//    //image_put(img);
//    
//    close_archives();
//    exit(0);
//}
