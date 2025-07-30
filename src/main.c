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


int __log_verbosity = 5;
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


struct linker_ctx
{
    struct list_head input_files;
    struct list_head archives;
    struct symtab *symtab;              // global symbol table
    struct list_head unresolved;        // queue of unresolved symbols
};


struct unresolved
{
    struct list_head list;
    struct symbol *symbol;
};


static bool record_symbol(void *cb_data, struct objfile *objfile, const struct syminfo *info)
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

        rc = symbol_alloc(&sym, objfile, objfile->name, info->weak);
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

        int rc = symbol_alloc(&symbol, objfile, info->name, info->weak);
        if (rc != 0) {
            return false;
        }

        struct symbol *exist;
        rc = symtab_insert_symbol(ctx->symtab, symbol, &exist);
        if (rc == EEXIST) {
            if (!exist->weak) {
                log_error("Multiple definitions for symbol '%s'; both %s and %s define it",
                        symbol->name, objfile->name, symbol_referer(exist)->name);
                symbol_free(symbol);
                return false;
            }

            // FIXME: prefer the largest one (because of common)
            log_debug("Replacing weak symbol '%s' with definition from %s",
                    symbol->name, objfile->name);

            // The existing symbol is weak, replace it
            symtab_replace_symbol(ctx->symtab, exist, symbol);
            symbol_free(exist);

        } else if (rc != 0) {
            // Something else went wrong
            symbol_free(symbol);
            return false;
        }
    }

    if (info->section != NULL) {
        // This is a symbol definition, attempt to resolve it
        int rc = symbol_link_definition(symbol, info->section, info->offset);
        if (rc != 0) {
            log_error("Multiple definitions for symbol '%s'; both %s and %s define it",
                    symbol->name, objfile->name, symbol->objfile->name);
            return false;
        }

    } else {
        assert(info->is_reference);

        // This is a symbol reference, if we didn't just create it, 
        // add ourselves as a referer. Otherwise, add it to the list
        // of unresolved symbols
        if (symbol_referer(symbol) != objfile) {
            symbol_add_reference(symbol, objfile);

        } else if (symbol_is_undefined(symbol)) {
            struct unresolved *unresolved = malloc(sizeof(struct unresolved));
            if (unresolved == NULL) {
                return false;
            }
            
            unresolved->symbol = symbol;
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
    return ctx;
}


static void destroy_ctx(struct linker_ctx *ctx)
{
    list_for_each_entry_safe(file, &ctx->input_files, struct input_file, list) {
        close_input_file(file);
    }

    list_for_each_entry_safe(file, &ctx->archives, struct archive_file, list) {
        close_archive(file);
    }

    list_for_each_entry_safe(unresolved, &ctx->unresolved, struct unresolved, list) {
        list_remove(&unresolved->list);
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
    int idx = 0;

    static struct option options[] = {
        //{"vm", required_argument, 0, 'i'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };

    //const char *vmpath = DEFAULT_BFVM;

    while ((c = getopt_long_only(argc, argv, ":h", options, &idx)) != -1) {
        switch (c) {
            //case 'i':
            //    vmpath = optarg;
            //    break;

            case 'h':
                fprintf(stdout, "Usage: %s [--vm objfile] objfile...\n", argv[0]);
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
            log_fatal("Failed to extract symbols");
            log_ctx_pop();
            destroy_ctx(ctx);
            exit(1);
        }
        log_ctx_pop();
    }

    // Resolve undefined symbols
    while (!list_empty(&ctx->unresolved)) {
        struct unresolved *unresolved = list_first_entry(&ctx->unresolved, struct unresolved, list);

        // This is necessary for the first pass since we've only gone through the object files once
        if (!symbol_is_undefined(unresolved->symbol)) {
            // symbol is no longer unresolved, just ignore it
            list_remove(&unresolved->list);
            free(unresolved);
            continue;
        }

        log_ctx_push(LOG_CTX_FILE(NULL, symbol_referer(unresolved->symbol)->name));

        // Try to look up symbol in any of the archives
        list_for_each_entry(ar, &ctx->archives, struct archive_file, list) {
            struct archive_symbol *found = archive_lookup_symbol(ar->file, unresolved->symbol->name);
            if (found) {
                log_debug("Found unresolved symbol '%s' in %s",
                        unresolved->symbol->name, ar->file->name);

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
            exit(1);

        } else {
            list_remove(&unresolved->list);
            free(unresolved);
        }

        log_ctx_pop();
    }

    destroy_ctx(ctx);
    exit(0);
}
