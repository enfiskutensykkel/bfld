#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <logging.h>
#include <getopt.h>
#include <mfile.h>
#include <linker.h>
#include <assert.h>

#include <utils/list.h>
#include <utils/rbtree.h>
#include <symbols.h>
#include <globals.h>
#include <symbol.h>

#include <objfile.h>
#include <archive.h>
#include <objfile_frontend.h>
#include <archive_frontend.h>


static void print_symbols(FILE *fp, struct linkerctx *ctx)
{
    static const char *typemap[] = {
        [SYMBOL_NOTYPE] = "notype",
        [SYMBOL_OBJECT] = "object",
        [SYMBOL_TLS] = "tls",
        [SYMBOL_SECTION] = "sect",
        [SYMBOL_FUNCTION] = "func"
    };

    static const char *bindmap[] = {
        [SYMBOL_WEAK] = "weak",
        [SYMBOL_GLOBAL] = "global",
        [SYMBOL_LOCAL] = "local"
    };

    struct rb_node *node = rb_first(&ctx->globals->map);

    fprintf(fp, "%6s %-16s %6s %6s %-6s %-6s %1s %-32s\n",
            "Offset", "Value", "Size", "Align", "Type", "Bind", "D", "Name");

    while (node != NULL) {
        const struct symbol *sym = rb_entry(node, struct globals_entry, map_entry)->symbol;
        node = rb_next(node);

        char def = 'U';

        if (sym->section != NULL) {
            def = 'D';
        } else if (sym->is_absolute) {
            def = 'A';
        } else if (sym->is_common) {
            def = 'C';
        }

        fprintf(fp, "%6lu ", sym->offset);
        fprintf(fp, "%016lx ", sym->value);
        fprintf(fp, "%6lu ", sym->size);
        fprintf(fp, "%6lu ", sym->align);
        fprintf(fp, "%-6s ", typemap[sym->type]);
        fprintf(fp, "%-6s ", bindmap[sym->binding]);
        fprintf(fp, "%c ", def);
        fprintf(fp, "%-32.32s", sym->name);
        fprintf(fp, "\n");
    }
}


//static void print_relocs(FILE *fp, struct linkerctx *ctx)
//{
//    list_for_each_entry(entry, &ctx->processed, struct input_file, list_entry) {
//        const struct sections *sections = entry->sections;
//
//        fprintf(fp, "Section table '%s' contains %zu entries:\n",
//                sections->name, sections->nsections);
//
//        for (size_t i = 0, n = 0; i < sections->capacity && n < sections->nsections; ++i) {
//            const struct section *sect = sections_at(sections, i);
//
//            if (sect == NULL) {
//                continue;
//            }
//
//            fprintf(fp, "Section %zu '%s' has %zu relocations:\n", i, sect->name, sect->nrelocs);
//
//            fprintf(fp, "%6s %-2s %6s %6s %-32s\n",
//                    "Num", "T", "Offset", "Addend", "Symbol");
//
//            size_t j = 0;
//            list_for_each_entry(reloc, &sect->relocs, const struct reloc, list_entry) {
//                fprintf(fp, "%6zu ", j++);
//                fprintf(fp, "%02x ", reloc->type);
//                fprintf(fp, "%6lu ", reloc->offset);
//                fprintf(fp, "%6ld ", reloc->addend);
//                fprintf(fp, "%-32.32s", reloc->symbol->name);
//                fprintf(fp, "\n");
//            }
//
//            ++n;
//        }
//        fprintf(fp, "\n");
//    }
//}


static char * format_option(char *buf, 
                            size_t bufsz, 
                            int has_arg, 
                            const char *optname, 
                            const char *argname)
{
    size_t i = 0;

    if (bufsz == 0) {
        return buf + i;
    }

    for (size_t j = 0; i < bufsz && optname[j] != '\0'; ++i, ++j) {
        buf[i] = optname[j];
    }

    if (i == bufsz) {
        return buf + i;
    }

    if (has_arg == no_argument) {
        return buf + i;
    }
    assert(argname != NULL);

    if (has_arg == optional_argument) {
        buf[i++] = '[';
        if (i == bufsz) {
            return buf + i;
        }
    }

    buf[i++] = has_arg == optional_argument ? '=' : ' ';
    if (i == bufsz) {
        return buf + i;
    }

    for (size_t j = 0; i < bufsz && argname[j] != '\0'; ++i, ++j) {
        buf[i] = argname[j];
    }
    if (i == bufsz) {
        return buf + i;
    }

    if (has_arg == optional_argument) {
        buf[i++] = ']';
        if (i == bufsz) {
            return buf + i;
        }
    }

    return buf + i;
}


static const char * next_word(const char *s)
{
    while (*s != '\0' && *s < '!') {
        ++s;
    }

    return *s != '\0' ? s : NULL;
}


static int wordlen(const char *s)
{
    int n = 0;

    while (s[n] != '\0' && s[n] >= '!') {
        ++n;
    }

    return n;
}


static void print_option(FILE *fp,
                         const char *shortname,
                         const char *longname,
                         int has_arg,
                         const char *argname,
                         const char *help)
{
    int col = 32;
    char buffer[128];
    memset(buffer, ' ', 2);
    char *s = &buffer[0] + 2;

    if (argname == NULL) {
        has_arg = no_argument;
    }

    if (shortname) {
        s = format_option(s, sizeof(buffer) - (s - &buffer[0]),
                          has_arg, shortname, argname);

        if (longname) {
            s += snprintf(s, sizeof(buffer) - (s - buffer), ", ");
        }
    }

    if (longname) {
        s = format_option(s, sizeof(buffer) - (s - buffer),
                has_arg, longname, argname);
    }

    if (sizeof(buffer) - (s - buffer) > 0) {
        *s = '\0';
    }

    int n = strnlen(buffer, sizeof(buffer));
    fprintf(fp, "%.*s", n, buffer);

    memset(buffer, ' ', col);
    buffer[col] = '\0';

    if (n < col) {
        fprintf(fp, "%.*s", col - n, buffer);
    } else {
        fprintf(fp, "\n%s", buffer);
    }
    int curr = col;

    const char *word = next_word(help);

    while (word != NULL) {
        n = wordlen(word);
        
        if (curr + n > 79) {
            curr = col;
        }
        curr += n + 1;
        fprintf(fp, " %.*s", n, word);
        word = next_word(word + n);
    }
    fprintf(fp, "\n");
}


static bool linker_load_file(struct linkerctx *ctx, const char *pathname)
{
    struct mfile *file = NULL;

    log_ctx_new(pathname);

    int status = mfile_open_read(&file, pathname);
    if (status != 0) {
        log_ctx_pop();
        return false;
    }

    // Try to open as archive file
    const struct archive_frontend *arfe = archive_frontend_probe(file->data, file->size);
    if (arfe != NULL) {
        struct archive *ar = archive_alloc(file, file->name, file->data, file->size);

        if (ar != NULL) {
            bool success = linker_add_archive(ctx, ar, arfe);
            archive_put(ar);
            mfile_put(file);
            log_ctx_pop();
            return success;
        }
    }

    // Try to open as object file
    const struct objfile_frontend *objfe = objfile_frontend_probe(file->data, file->size, NULL);
    if (objfe != NULL) {
        struct objfile *obj = objfile_alloc(file, file->name, file->data, file->size);

        if (obj != NULL) {
            bool success = linker_add_input_file(ctx, obj, objfe);
            objfile_put(obj);
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



int main(int argc, char **argv)
{
    int c;
    int idx = -1;
    static int dump_symbols = 0;
    static int dump_relocs = 0;

    const char *output_file = "a.out";
    const char *entry = "_start";

    static struct option options[] = {
        {"verbose", optional_argument, 0, 'v'},
        {"help", no_argument, 0, 'h'},
        {"output", required_argument, 0, 'o'},
        {"entry", required_argument, 0, 'e'},
        {"dump-symbols", no_argument, &dump_symbols, 10},
        {"dump-relocs", no_argument, &dump_relocs, 1},
        {0, 0, 0, 0}
    };

    struct linkerctx *ctx = linker_create(argv[0]);
    if (ctx == NULL) {
        exit(2);
    }

    while ((c = getopt_long_only(argc, argv, ":hvo:e:", options, &idx)) != -1) {
        switch (c) {

            case 0:
                break;

            case 'o':
                output_file = optarg;
                break;

            case 'e':
                entry = optarg;
                break;

            case 'v':
                if (optarg == NULL) {
                    ++log_level;
                } else {
                    char *endptr = NULL;
                    int verbosity = strtol(optarg, &endptr, 10);
                    if (*endptr != '\0') {
                        log_error("Invalid log level: '%s'", optarg);
                        linker_destroy(ctx);
                        exit(1);
                    }
                    log_level = verbosity;
                }
                break;

            case 'h':
                fprintf(stdout, "Usage: %s [OPTIONS] FILE...\n", argv[0]);
                fprintf(stdout, "Options:\n");
                print_option(stdout, "-h", "--help", no_argument, NULL, "Show this help and quit.");
                print_option(stdout, "-v", "--verbose", optional_argument, "level", "Increase log level.");
                print_option(stdout, "-o", "--output", required_argument, "FILE", "Set output file name.");
                print_option(stdout, "-e", "--entry", required_argument, "ADDRESS", "Set start address.");
                print_option(stdout, "--dump-symbols", NULL, no_argument, NULL, "Print symbols.");
                print_option(stdout, "--dump-relocs", NULL, no_argument, NULL, "Print relocations.");
                linker_destroy(ctx);
                exit(0);

            case ':':
                log_error("Missing value for option '%s'", argv[optind-1]);
                linker_destroy(ctx);
                exit(1);

            default:
                log_error("Unrecognized option '%s'", argv[optind-1]);
                linker_destroy(ctx);
                exit(1);
        }
    }

    if (optind >= argc) {
        log_error("No input files");
        linker_destroy(ctx);
        exit(1);
    }

    for (int i = optind; i < argc; ++i) {
        bool success = linker_load_file(ctx, argv[i]);
        if (!success) {
            linker_destroy(ctx);
            exit(2);
        }
    }

    if (!linker_resolve_globals(ctx)) {
        linker_destroy(ctx);
        exit(3);
    }

    if (dump_symbols) {
        print_symbols(stdout, ctx);
    }

    // Identify sections that we need to keep
    struct symbol *ep = globals_find_symbol(ctx->globals, entry);
    if (ep == NULL) {
        log_fatal("Undefined reference to '%s'", entry);
        linker_destroy(ctx);
        exit(3);
    }

    linker_create_common_section(ctx);

    linker_keep_section(ctx, ep->section);
    linker_gc_sections(ctx);

    linker_destroy(ctx);
    exit(0);
}
