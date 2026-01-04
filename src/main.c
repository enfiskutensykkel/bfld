#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <logging.h>
#include <getopt.h>
#include <mfile.h>
#include <linker.h>
#include <objfile_frontend.h>
#include <archive_frontend.h>


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
            log_fatal("Could not open all files");
            linker_destroy(ctx);
            exit(2);
        }

    }

    linker_destroy(ctx);
    exit(0);
}
