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
#include <image.h>


static void print_symbols(FILE *fp, struct image *img)
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

    fprintf(fp, "%-16s %6s %6s %-6s %-6s %-20s %-32s\n",
            "Value", "Size", "Align", "Type", "Bind", "Definition", "Name");

    for (uint64_t idx = 1; idx <= img->symbols.nsymbols; ++idx) {
        const struct symbol *sym = symbols_at(&img->symbols, idx);

        if (sym->type == SYMBOL_SECTION) {
            continue;
        }

        const char *defname = "UNKNOWN";
        if (!symbol_is_defined(sym)) {
            defname = "UNDEFINED";
        } else if (sym->is_absolute) {
            defname = "ABSOLUTE";
        } else if (sym->section != NULL && sym->section->name != NULL) {
            defname = sym->section->name;
        }

        fprintf(fp, "%016lx ", sym->value);
        fprintf(fp, "%6lu ", sym->size);
        fprintf(fp, "%6lu ", sym->align);
        fprintf(fp, "%-6s ", typemap[sym->type]);
        fprintf(fp, "%-6s ", bindmap[sym->binding]);
        fprintf(fp, "%-20.20s ", defname);
        fprintf(fp, "%-32.32s", sym->name);
        fprintf(fp, "\n");
    }
}


static void print_layout(FILE *fp, struct image *img)
{
    fprintf(fp, "Memory layout for image '%s\n", img->name);
    fprintf(fp, "Base address: 0x%016lx\n", img->base_addr);
    fprintf(fp, "Entry point : 0x%016lx\n", img->entrypoint);
    fprintf(fp, "Memory size : %lu\n", img->size);

    fprintf(fp, "Output sections:\n");
    list_for_each_entry(grp, &img->groups, struct section_group, list_entry) {
        fprintf(fp, "-- Addr=0x%016lx, Size=%06lu, Section='%s'\n", grp->vaddr, grp->size, grp->name);
        
        fprintf(fp, "   Input sections:\n");
        for (uint64_t idx = 1; idx <= grp->sections.nsections; ++idx) {
            const struct section *sect = sections_at(&grp->sections, idx);
            fprintf(fp, "     [%06lu] Addr=0x%016lx, Size=%06lu, Section='%s'\n", 
                    idx, sect->vaddr, sect->size, sect->name);
        }
    }
}


static void keep_section(struct sections *keep, struct section *sect)
{
    sections_push(keep, sect);
    sect->is_alive = true;
}


static void keep_symbol(struct sections *keep, struct symbol *sym)
{
    sym->is_used = true;
    if (symbol_is_defined(sym)) {
        if (sym->section != NULL) {
            keep_section(keep, sym->section);
        } else {
            // find section with address
        }
    }
}



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
    static int show_symbols = 0;
    static int show_layout = 0;

    const char *output_file = "a.out";
    const char *entry = "_start";

    static struct option options[] = {
        {"verbose", optional_argument, 0, 'v'},
        {"help", no_argument, 0, 'h'},
        {"output", required_argument, 0, 'o'},
        {"entry", required_argument, 0, 'e'},
        {"show-symbols", no_argument, &show_symbols, 10},
        {"show-layout", no_argument, &show_layout, 1},
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
                print_option(stdout, "--show-symbols", NULL, no_argument, NULL, "Print global symbol table.");
                print_option(stdout, "--show-layout", NULL, no_argument, NULL, "Print image layout information.");
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

    // Identify sections that we need to keep
    struct symbol *ep = globals_find_symbol(ctx->globals, entry);
    if (ep == NULL) {
        log_fatal("Undefined reference to '%s'", entry);
        linker_destroy(ctx);
        exit(3);
    }

    struct sections keep = {0};

    keep_symbol(&keep, ep);
    
    linker_gc_sections(ctx, &keep);
    sections_clear(&keep);

    struct image *img = linker_create_image(ctx, output_file, 0x400000);
    img->entrypoint = ep->value;

    if (show_symbols) {
        print_symbols(stdout, img);
    }

    linker_destroy(ctx);

    if (show_layout) {
        print_layout(stdout, img);
    }

    image_put(img);
    exit(0);
}
