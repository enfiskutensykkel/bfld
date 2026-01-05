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


static void print_symbol(FILE *fp, size_t idx, const struct symbol *sym)
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

    fprintf(fp, "%6zu ", idx);
    fprintf(fp, "%3s ", symbol_is_defined(sym) ? "yes" : "no");
    fprintf(fp, "%016lx ", sym->value);
    fprintf(fp, "%6lu ", sym->size);
    fprintf(fp, "%6lu ", sym->align);
    fprintf(fp, "%-6s ", typemap[sym->type]);
    fprintf(fp, "%-6s ", sym->is_common ? "yes" : "no");
    fprintf(fp, "%-6s ", bindmap[sym->binding]);
    fprintf(fp, "%-32.32s", sym->name);
    fprintf(fp, "\n");
}

static void print_symbols(FILE *fp, struct linkerctx *ctx)
{
    fprintf(fp, "Global symbol table '%s' contains %zu entries:\n",
            ctx->globals->name, ctx->globals->nsymbols);

    size_t n = 0;
    fprintf(fp, "%6s %-3s %-16s %6s %6s %-6s %6s %-6s %-32s\n",
            "Num", "Def", "Value", "Size", "Align", "Type", "Common", "Bind", "Name");

    for (const struct rb_node *node = rb_first(&ctx->globals->map);
            node != NULL;
            node = rb_next(node)) {
        const struct globals_entry *entry = rb_entry(node, struct globals_entry, map_entry);
        const struct symbol *sym = entry->symbol;
        print_symbol(fp, n++, sym);
    }

    list_for_each_entry(entry, &ctx->input_files, struct input_file, list_entry) {
        const struct symbols *symbols = entry->symbols;
        fprintf(fp, "\nLocal symbol table '%s' contains %zu entries:\n",
                symbols->name, symbols->nsymbols);
        fprintf(fp, "%6s %-3s %-16s %6s %6s %-6s %6s %-6s %-32s\n",
                "Num", "Def", "Value", "Size", "Align", "Type", "Common", "Bind", "Name");
        for (n = 0; n < symbols->capacity; ++n) {
            const struct symbol *sym = symbols->entries[n];
            if (sym != NULL) {
                print_symbol(fp, n, sym);
            }
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

    buf[i++] = '=';
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



int main(int argc, char **argv)
{
    int c;
    int idx = -1;
    static int dump_symbols = 0;

    static struct option options[] = {
        {"verbose", optional_argument, 0, 'v'},
        {"help", no_argument, 0, 'h'},
        {"dump-symbols", no_argument, &dump_symbols, 10},
        {0, 0, 0, 0}
    };

    struct linkerctx *ctx = linker_create(argv[0]);
    if (ctx == NULL) {
        exit(2);
    }

    while ((c = getopt_long_only(argc, argv, ":hv", options, &idx)) != -1) {
        switch (c) {

            case 0:
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
                fprintf(stdout, "Usage: %s [options] file...\n", argv[0]);
                fprintf(stdout, "Options:\n");
                print_option(stdout, "-h", "--help", no_argument, NULL, "Show this help and quit.");
                print_option(stdout, "-v", "--verbose", optional_argument, "level", "Increase log level.");
                print_option(stdout, "--dump-symbols", NULL, no_argument, NULL, "Dump symbols.");
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

    if (dump_symbols) {
        print_symbols(stdout, ctx);
        linker_destroy(ctx);
        exit(0);
    }

    linker_destroy(ctx);
    exit(0);
}
