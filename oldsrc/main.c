#include <merge.h>
#include <archive.h>
#include <logging.h>
#include <mfile.h>
#include <objfile.h>
#include <objfile_loader.h>
#include <symtab.h>
#include <utils/list.h>
#include <utils/rbtree.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <errno.h>
#include <assert.h>
#include <align.h>


int __log_verbosity = 1;
int __log_ctx_idx = 0;
log_ctx_t __log_ctx[LOG_CTX_NUM] = {0};


struct linker_ctx;


struct input_file
{
    struct linker_ctx *ctx;
    struct list_head list;
    struct objfile *file;
    struct symtab *symtab;  // local symbol table
};


struct archive_file
{
    struct linker_ctx *ctx;
    struct list_head list;
    struct archive *file;
};


struct output_section
{
    struct linker_ctx *ctx;
    struct list_head list;
    struct merged_section *sect;
};


struct linker_ctx
{
    struct list_head input_files;
    struct list_head archives;          // list of archives to pull members from
    struct list_head output_sects;      // output sections (merged)
    struct symtab *symtab;              // global symbol table
    struct list_head unresolved;        // queue of unresolved symbols
};


struct unresolved
{
    struct list_head list;
    struct symbol *symbol;
    struct objfile *objfile;
};


//static void resolve_symbols(struct symtab *symtab)
//{
//    struct rb_node *node = rb_first(&symtab->tree);
//
//    while (node != NULL) {
//        struct symbol *sym = rb_entry(node, struct symbol, tree_node);
//        int status = symbol_resolve_address(sym);
//        if (status != 0) {
//            log_error("Could not resolve symbol '%s' in symbol table %s",
//                        sym->name, symtab->name);
//        }
//        node = rb_next(node);
//
//        log_debug("Symbol '%s' finalized address 0x%lx", sym->name, sym->addr);
//    }
//}


static bool record_reloc(void *cb_data, struct objfile *objfile, const struct reloc_info *info)
{
    if (info->target != NULL) {
        fprintf(stderr, "%s\n", info->target->name);
    } else {
        fprintf(stderr, "%s\n", info->symbol_name);
    }

    return true;
}


static bool record_symbol(void *cb_data, struct objfile *objfile, const struct symbol_info *info)
{
    // FIXME: treat hidden/internal as local
    struct input_file *file = cb_data;
    struct linker_ctx *ctx = file->ctx;

    if (!info->global && info->is_reference) {
        log_error("Local symbol '%s' is undefined", info->name);
        return false;
    }

    if (!info->global) {
        int rc;
        struct symbol *sym;

        rc = symbol_alloc(&sym, info->name, info->weak, info->relative);
        if (rc != 0) {
            return false;
        }

        sym->type = info->type;
        if (info->section != NULL) {
            rc = symbol_link_definition(sym, info->section, info->offset);
            assert(rc == 0);
        }

        rc = symtab_insert_symbol(file->symtab, sym, NULL);
        if (rc != 0) {
            log_error("Symbol '%s' is defined multiple times in %s",
                    sym->name, objfile->name);
            symbol_free(sym);
            return false;
        }

        return true;
    }

    struct symbol *symbol = symtab_find_symbol(ctx->symtab, info->name);
    if (symbol == NULL) {
        // This is a new global symbol

        int rc = symbol_alloc(&symbol, info->name, info->weak, info->relative);
        if (rc != 0) {
            return false;
        }

        struct symbol *exist;
        rc = symtab_insert_symbol(ctx->symtab, symbol, &exist);
        if (rc == EEXIST) {
            if (!exist->weak) {
                log_error("Multiple definitions for symbol '%s'; first defined in %s",
                        symbol->name, symbol->objfile->name);
                symbol_free(symbol);
                return false;
            }

            // FIXME: prefer the largest one (because of common)
            log_debug("Replacing weak symbol '%s' previously defined in %s",
                    symbol->name, symbol->objfile->name);

            // The existing symbol is weak, replace it
            symtab_replace_symbol(ctx->symtab, exist, symbol);
            symbol_free(exist);

        } else if (rc != 0) {
            // Something else went wrong
            symbol_free(symbol);
            return false;
        }
    }

    if (info->section != NULL || !info->relative) {
        // This is a symbol definition, attempt to resolve it
        int rc = symbol_link_definition(symbol, info->section, info->offset);
        if (rc != 0) {
            log_error("Multiple definitions for symbol '%s'; first defined in %s",
                    symbol->name, symbol->objfile->name);
            return false;
        }

    } else {
        assert(info->is_reference);

        // This is a symbol reference
        // If the symbol is undefined, add it to the list of unresolved symbols
        if (symbol_is_undefined(symbol)) {
            struct unresolved *unresolved = malloc(sizeof(struct unresolved));
            if (unresolved == NULL) {
                return false;
            }
            
            unresolved->symbol = symbol;
            unresolved->objfile = objfile;
            objfile_get(objfile);
            list_insert_tail(&ctx->unresolved, &unresolved->list);
        }
    }

    return true;
}


static struct input_file * insert_objfile(struct linker_ctx *ctx, struct objfile *objfile)
{
    if (objfile == NULL) {
        return NULL;
    }

    struct input_file *handle = malloc(sizeof(struct input_file));
    if (handle == NULL) {
        objfile_put(objfile);
        return NULL;
    }

    int status = symtab_init(&handle->symtab, objfile->name);
    if (status != 0) {
        free(handle);
        objfile_put(objfile);
        return NULL;
    }

    handle->file = objfile;
    list_insert_tail(&ctx->input_files, &handle->list);
    handle->ctx = ctx;

    return handle;
}


static struct input_file * try_open_input_file(struct linker_ctx *ctx, mfile *file)
{
    struct objfile *objfile;

    objfile = objfile_load(file, NULL);
    if (objfile == NULL) {
        return NULL;
    }

    return insert_objfile(ctx, objfile);
}


static struct archive_file * try_open_archive(struct linker_ctx *ctx, mfile *file)
{
    struct archive *ar;

    ar = archive_load(file, NULL);
    if (ar == NULL) {
        return NULL;
    }

    struct archive_file *handle = malloc(sizeof(struct archive_file));
    if (handle == NULL) {
        archive_put(ar);
        return NULL;
    }

    handle->file = ar;
    list_insert_tail(&ctx->archives, &handle->list);
    handle->ctx = ctx;
    return handle;
}


static void close_archive(struct archive_file *file)
{
    list_remove(&file->list);
    archive_put(file->file);
    free(file);
}


static void close_input_file(struct input_file *file)
{
    list_remove(&file->list);
    symtab_put(file->symtab);
    objfile_put(file->file);
    free(file);
}


static struct linker_ctx * create_ctx(void)
{
    struct linker_ctx *ctx = malloc(sizeof(struct linker_ctx));
    if (ctx == NULL) {
        return NULL;
    }

    int status = symtab_init(&ctx->symtab, "global");
    if (status != 0) {
        free(ctx);
        return NULL;
    }

    list_head_init(&ctx->input_files);
    list_head_init(&ctx->archives);
    list_head_init(&ctx->unresolved);
    list_head_init(&ctx->output_sects);
    return ctx;
}


static void destroy_ctx(struct linker_ctx *ctx)
{
    list_for_each_entry_safe(sect, &ctx->output_sects, struct output_section, list) {
        list_remove(&sect->list);
        merged_section_put(sect->sect);
        free(sect);
    }

    list_for_each_entry_safe(file, &ctx->input_files, struct input_file, list) {
        close_input_file(file);
    }

    list_for_each_entry_safe(file, &ctx->archives, struct archive_file, list) {
        close_archive(file);
    }

    list_for_each_entry_safe(unresolved, &ctx->unresolved, struct unresolved, list) {
        list_remove(&unresolved->list);
        objfile_put(unresolved->objfile);
        free(unresolved);
    }

    symtab_put(ctx->symtab);
    free(ctx);
}


/*
 * Basic linker operation
 * - Read input object files. Determine length and type of the contents and read symbols.
 * - Build symbol table containing all the symbols, linking undefined symbols to their definitions.
 * - Decide where all contents should go in the output executable. Decide where in memory they
 *   should go when program runs.
 * - Read the contents data and relocations. Apply the relocations to contents, and write to output file.
 * - Optionally write out the complete symbol table with the final values of the symbols.
 */


int main(int argc, char **argv)
{
    __log_ctx[0] = LOG_CTX(argv[0]);
    int c;
    int idx = -1;

    static struct option options[] = {
        //{"vm", required_argument, 0, 'i'},
        {"verbose", optional_argument, 0, 'v'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };

    //const char *vmpath = DEFAULT_BFVM;

    while ((c = getopt_long_only(argc, argv, ":hv", options, &idx)) != -1) {
        switch (c) {
            //case 'i':
            //    vmpath = optarg;
            //    break;

            case 'v':
                if (optarg == NULL) {
                    ++__log_verbosity;
                } else {
                    char *endptr = NULL;
                    int verbosity = strtol(optarg, &endptr, 10);
                    if (*endptr != '\0') {
                        fprintf(stderr, "%d\n", idx);
                        log_error("Invalid verbosity: %s", options[idx].name, optarg);
                        exit(1);
                    }
                    __log_verbosity = verbosity;
                }
                break;

            case 'h':
                fprintf(stdout, "Usage: %s [-v] [-vv] [-vvv] [--vm objfile] objfile...\n", argv[0]);
                exit(0);

            case ':':
                log_error("Missing value for option %s", argv[optind-1]);
                exit(1);

            default:
                log_error("Unknown option %s", argv[optind-1]);
                exit(1);
        }
    }

    if (optind >= argc) {
        log_error("Missing argument objfile");
        exit(1);
    }

    struct linker_ctx *ctx = create_ctx();

    for (int i = optind; i < argc; ++i) {
        mfile *file = NULL;
        int status = mfile_open_read(&file, argv[i]);

        if (status != 0) {
            log_fatal("Could not open all input files");
            destroy_ctx(ctx);
            exit(2);
        }

        log_ctx_push(LOG_CTX_FILE(NULL, file->name));

        if (try_open_input_file(ctx, file) != NULL) {
            // file is an object file
        } else if (try_open_archive(ctx, file) != NULL) {
            // file is an archive
        } else {
            log_fatal("Unrecognized file format");
            mfile_put(file);
            destroy_ctx(ctx);
            log_ctx_pop();
            exit(2);
        }

        mfile_put(file);
        log_ctx_pop();
    }

    // Extract symbols and build symbol tables
    list_for_each_entry(ifile, &ctx->input_files, struct input_file, list) {
        log_ctx_push(LOG_CTX_FILE(NULL, ifile->file->name));
        int status = objfile_extract_symbols(ifile->file, record_symbol, ifile);
        if (status != 0) {
            log_ctx_pop();
            destroy_ctx(ctx);
            exit(1);
        }
        log_ctx_pop();
    }


    // Resolve symbol definitions
    while (!list_empty(&ctx->unresolved)) {
        struct unresolved *unresolved = list_first_entry(&ctx->unresolved, struct unresolved, list);

        // This is necessary for the first pass since we've only gone through the object files once
        if (!symbol_is_undefined(unresolved->symbol)) {
            // symbol is no longer unresolved, just ignore it
            list_remove(&unresolved->list);
            objfile_put(unresolved->objfile);
            free(unresolved);
            continue;
        }

        log_ctx_push(LOG_CTX_FILE(NULL, unresolved->objfile->name));

        // Try to look up symbol in any of the archives
        list_for_each_entry(ar, &ctx->archives, struct archive_file, list) {
            struct archive_symbol *found = archive_lookup_symbol(ar->file, unresolved->symbol->name);
            if (found) {

                struct input_file *file = insert_objfile(ctx, archive_load_member_objfile(found->member));
                if (file == NULL) {
                    log_error("Failed to load archive member");
                    continue;
                }

                int status = objfile_extract_symbols(file->file, record_symbol, file);
                if (status != 0) {
                    log_fatal("Failed to extract symbols");
                    destroy_ctx(ctx);
                    continue;
                }
                break;
            }
        }

        if (symbol_is_undefined(unresolved->symbol)) {
            log_fatal("Reference to undefined symbol '%s'", 
                    unresolved->symbol->name);
            log_ctx_pop();
            destroy_ctx(ctx);
            exit(2);

        } else {
            list_remove(&unresolved->list);
            objfile_put(unresolved->objfile);
            free(unresolved);
        }

        log_ctx_pop();
    }

    // Extract relocations
    list_for_each_entry(ifile, &ctx->input_files, struct input_file, list) {
        log_ctx_push(LOG_CTX_FILE(NULL, ifile->file->name));

        int status = objfile_extract_relocs(ifile->file, record_reloc, ifile);
        if (status != 0) {
            log_ctx_pop();
            destroy_ctx(ctx);
            exit(1);
        }

        log_ctx_pop();
    }

    // Merge sections of the same type
    enum section_type secttypes[] = {
        SECTION_TEXT, SECTION_RODATA, SECTION_DATA, SECTION_ZERO
    };

    const char *sectnames[] = {
        ".text", ".rodata", ".data", ".bss"
    };

    for (size_t idx = 0; idx < sizeof(secttypes) / sizeof(enum section_type); ++idx) {
        enum section_type type = secttypes[idx];
        const char *name = sectnames[idx];

        struct output_section *outsect = malloc(sizeof(struct output_section));
        if (outsect == NULL) {
            destroy_ctx(ctx);
            exit(3);
        }

        int status = merged_section_init(&outsect->sect, name, type);
        if (status != 0) {
            free(outsect);
            destroy_ctx(ctx);
            exit(3);
        }

        list_insert_tail(&ctx->output_sects, &outsect->list);

        list_for_each_entry(ifile, &ctx->input_files, struct input_file, list) {
            const struct objfile *objfile = ifile->file;

            const struct rb_node *node = rb_first(&objfile->sections);

            while (node != NULL) {
                struct section *sect = rb_entry(node, struct section, tree_node);
                if (sect->type == outsect->sect->type) {
                    merged_section_add_section(outsect->sect, sect);
                }

                node = rb_next(node);
            }
        }
    }

    // Resolve addresses
    uint64_t base_addr = 0x400000;
    list_for_each_entry(outsect, &ctx->output_sects, struct output_section, list) {

        merged_section_finalize_addresses(outsect->sect, BFLD_ALIGN(base_addr, outsect->sect->align));
        base_addr = outsect->sect->addr + outsect->sect->total_size;

        log_debug("Section %s 0x%lx - 0x%lx (size: %lu, align: %lu)",
                  outsect->sect->name, outsect->sect->addr,
                  outsect->sect->addr + outsect->sect->total_size - 1,
                  outsect->sect->total_size, outsect->sect->align);
    }

//    resolve_symbols(ctx->symtab);
//    list_for_each_entry(inputfile, &ctx->input_files, struct input_file, list) {
//        resolve_symbols(inputfile->symtab);
//    }

    destroy_ctx(ctx);
    exit(0);
}
