#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <logging.h>
#include <getopt.h>
#include <mfile.h>
#include <linker.h>

#include <utils/rbtree.h>
#include <globals.h>
#include <symbol.h>


static void dump_globals(struct linkerctx *ctx)
{
    struct rb_node *node = rb_first(&ctx->globals->map);

    fprintf(stdout, "%-32s  D  T  %-16s  %4s\n", "symbol", "address", "size");

    while (node != NULL) {
        struct globals_entry *entry = rb_entry(node, struct globals_entry, map_entry);
        fprintf(stdout, "%-32.32s  ", entry->symbol->name);
        if (symbol_is_defined(entry->symbol)) {
            if (entry->symbol->is_absolute) {
                fprintf(stdout, "A");
            } else {
                fprintf(stdout, "R");
            }
        } else {
            fprintf(stdout, "?");
        }

        char type = 'N';
        switch (entry->symbol->type) {
            case SYMBOL_NOTYPE:
                type = 'N';
                break;
            case SYMBOL_OBJECT:
                type = 'O';
                break;
            case SYMBOL_TLS:
                type = 'T';
                break;
            case SYMBOL_SECTION:
                type = 'S';
                break;
            case SYMBOL_FUNCTION:
                type = 'F';
                break;
            default:
                type = '?';
                break;
        }
        fprintf(stdout, "  %c", type);

        fprintf(stdout, "  %016lx", entry->symbol->value);
        fprintf(stdout, "  %4lu", entry->symbol->size);
        fprintf(stdout, "\n");
        node = rb_next(node);
    }
}


int main(int argc, char **argv)
{
    int c;
    int idx = -1;

    static struct option options[] = {
        {"verbose", optional_argument, 0, 'v'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };

    struct linkerctx *ctx = linker_create(argv[0]);
    if (ctx == NULL) {
        exit(2);
    }

    while ((c = getopt_long_only(argc, argv, ":hv", options, &idx)) != -1) {
        switch (c) {
            case 'v':
                if (optarg == NULL) {
                    ++log_level;
                } else {
                    char *endptr = NULL;
                    int verbosity = strtol(optarg, &endptr, 10);
                    if (*endptr != '\0') {
                        fprintf(stderr, "%d\n", idx);
                        log_error("Invalid verbosity: %s", options[idx].name, optarg);
                        linker_destroy(ctx);
                        exit(1);
                    }
                    log_level = verbosity;
                }
                break;

            case 'h':
                fprintf(stdout, "Usage: %s [-v] [-vv] [-vvv] objfile...\n", argv[0]);
                linker_destroy(ctx);
                exit(0);

            case ':':
                log_error("Missing value for option %s", argv[optind-1]);
                linker_destroy(ctx);
                exit(1);

            default:
                log_error("Unknown option %s", argv[optind-1]);
                linker_destroy(ctx);
                exit(1);
        }
    }

    if (optind >= argc) {
        log_error("Missing argument objfile");
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

    dump_globals(ctx);

    linker_destroy(ctx);
    exit(0);
}
