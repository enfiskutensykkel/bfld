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


int __log_verbosity = 2;
int __log_ctx_idx = 0;
log_ctx_t __log_ctx[LOG_CTX_NUM];


// Reopening files / opening the same file twice
// 1. Track opened files globally: Maintain a hash table or RB tree of all opened object/archive files, keyed by absolute path (or inode+device for even stronger matching)
//struct file_entry {
//    const char *path;             // normalized/realpath
//    struct objfile *obj;          // parsed representation
//    struct rb_node tree_node;
//};
//2. Normalize the path: Use realpath(3) to canonicalize file names before lookup/insert:
//char real[PATH_MAX];
//realpath(input_path, real);
//3. Reuse parsed data
//If the file is already loaded, return the existing objfile pointer instead of reopening or reparsing:
//struct objfile *load_or_get_cached_obj(const char *path) {
//    if ((entry = rb_find(file_table, path))) {
//        return entry->obj;
//    }
//
//    struct objfile *obj = load_objfile(path);
//    insert_file_entry(file_table, path, obj);
//    return obj;
//}



struct input_file
{
    struct list_head list;
    struct objfile *file;
    struct rb_tree symtab;  // local symbol table
};


struct linker_ctx
{
    struct list_head input_files;
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
        fprintf(stdout, "%s %s with %s binding [%s] size=%zu\n", 
                sym->name, type_names[sym->type], binding_names[sym->binding],
                sym->defined ? "defined" : "extern",
                sym->size);

        node = rb_next(node);
    }
}


static int open_input_file(struct linker_ctx *ctx, const char *filename)
{
    struct input_file *f = malloc(sizeof(struct input_file));
    if (f == NULL) {
        return ENOMEM;
    }

    log_ctx_push(LOG_CTX_FILE(filename));

    mfile *mf = NULL;
    int status = mfile_init(&mf, filename);
    if (status != 0) {
        free(f);
        return EBADF;
    }

    f->file = objfile_load(mf, NULL);
    if (f->file == NULL) {
        mfile_put(mf);
        free(f);
        return EBADF;
    }

    mfile_put(mf);

    rb_tree_init(&f->symtab);
    list_insert_tail(&ctx->input_files, &f->list);

    log_ctx_pop();

    return 0;
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
    rb_tree_init(&ctx->global_symtab);

    return ctx;
}


static void destroy_ctx(struct linker_ctx *ctx)
{
    list_for_each_entry(file, &ctx->input_files, struct input_file, list) {
        close_input_file(file);
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
        {"vm", required_argument, 0, 'i'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };

    const char *vmpath = DEFAULT_BFVM;

    while ((c = getopt_long_only(argc, argv, ":h", options, &idx)) != -1) {
        switch (c) {
            case 'i':
                vmpath = optarg;
                break;

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
        int status = open_input_file(ctx, argv[i]);
        if (status != 0) {
            destroy_ctx(ctx);
            exit(2);
        }
    }

    // Build symbol tables
    list_for_each_entry(inputfile, &ctx->input_files, struct input_file, list) {
        int status = objfile_extract_symbols(inputfile->file, &ctx->global_symtab, &inputfile->symtab);
        if (status != 0) {
            destroy_ctx(ctx);
            exit(3);
        }

        fprintf(stdout, "Local symbol table for %s\n", objfile_filename(inputfile->file));
        dump_symtab(&inputfile->symtab);
        fprintf(stdout, "\n");
    }

    fprintf(stdout, "Global symbol table\n");
    dump_symtab(&ctx->global_symtab);

    destroy_ctx(ctx);
    exit(0);
}
