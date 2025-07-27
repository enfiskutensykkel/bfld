#include <archive.h>
#include <logging.h>
#include <mfile.h>
#include <objfile.h>
#include <objfile_loader.h>
#include <symbol.h>
#include <utils/list.h>
#include <utils/rbtree.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <errno.h>


int __log_verbosity = 5;
int __log_ctx_idx = 0;
log_ctx_t __log_ctx[LOG_CTX_NUM];


struct input_file
{
    struct list_head list;
    struct objfile *file;
    struct rb_tree symtab;  // local symbol table
};


struct archive_file
{
    struct list_head list;
    struct archive *file;
};


struct linker_ctx
{
    struct list_head input_files;
    struct list_head archives;
    struct rb_tree global_symtab;   // global symbol table
};


static void dump_symtab(struct rb_tree *symtab)
{
    struct rb_node *node = rb_first(symtab);

    const char *binding_names[SYMBOL_WEAK + 1] = {
        "LOCAL", "GLOBAL", "WEAK"
    };

    const char *type_names[SYMBOL_FUNCTION + 1] = {
        "UNDEFINED", "NOTYPE", "DATA", "FUNCTION"
    };

    while (node != NULL) {
        struct symbol *sym = rb_entry(node, struct symbol, tree_node);
        fprintf(stdout, "%s: type=%s, binding=%s, size=%zu [%s]\n", 
                sym->name, type_names[sym->type], binding_names[sym->binding],
                sym->size,
                sym->defined ? "defined" : "extern");

        node = rb_next(node);
    }
}


static struct input_file * try_open_input_file(struct linker_ctx *ctx, mfile *file)
{
    struct objfile *objfile;

    objfile = objfile_load(file, NULL);
    if (objfile == NULL) {
        return NULL;
    }

    struct input_file *handle = malloc(sizeof(struct input_file));
    if (handle == NULL) {
        objfile_put(objfile);
        return NULL;
    }

    handle->file = objfile;
    rb_tree_init(&handle->symtab);
    list_insert_tail(&ctx->input_files, &handle->list);
    return handle;
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
    return handle;
}


static void close_archive(struct archive_file *file)
{
    list_remove(&file->list);
    archive_put(file->file);
    free(file);
}


static void destroy_symtab(struct rb_tree *symtab)
{
    struct rb_node *node = rb_first(symtab);

    while (node != NULL) {
        struct symbol *sym = rb_entry(node, struct symbol, tree_node);
        struct rb_node *next = rb_next(node);

        symbol_remove(symtab, &sym);

        node = next;
    }

    rb_tree_init(symtab);
}


static void close_input_file(struct input_file *file)
{
    list_remove(&file->list);
    destroy_symtab(&file->symtab);
    objfile_put(file->file);
    free(file);
}


static struct linker_ctx * create_ctx(void)
{
    struct linker_ctx *ctx = malloc(sizeof(struct linker_ctx));
    if (ctx == NULL) {
        return NULL;
    }

    list_head_init(&ctx->input_files);
    list_head_init(&ctx->archives);
    rb_tree_init(&ctx->global_symtab);

    return ctx;
}


static void destroy_ctx(struct linker_ctx *ctx)
{
    list_for_each_entry(file, &ctx->input_files, struct input_file, list) {
        close_input_file(file);
    }

    list_for_each_entry(file, &ctx->archives, struct archive_file, list) {
        close_archive(file);
    }

    destroy_symtab(&ctx->global_symtab);
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

    // Build symbol tables
    list_for_each_entry(inputfile, &ctx->input_files, struct input_file, list) {
        int status = objfile_extract_symbols(inputfile->file, &ctx->global_symtab, &inputfile->symtab);
        if (status != 0) {
            log_fatal("Fatal error occurred while loading symbols");
            destroy_ctx(ctx);
            exit(3);
        }

        fprintf(stdout, "Local symbol table for %s\n", inputfile->file->name);
        dump_symtab(&inputfile->symtab);
        fprintf(stdout, "\n");
    }

    fprintf(stdout, "Global symbol table\n");
    dump_symtab(&ctx->global_symtab);

    destroy_ctx(ctx);
    exit(0);
}
