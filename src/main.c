#include "mfile.h"
#include "objfile.h"
#include "objfile_loader.h"
#include "utils/list.h"
#include "utils/rbtree.h"
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <getopt.h>
#include <errno.h>


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
                fprintf(stderr, "Missing value for option %s\n", argv[optind-1]);
                exit(1);

            default:
                fprintf(stderr, "Unknown option %s\n", argv[optind-1]);
                exit(1);
        }
    }

    if (optind >= argc) {
        fprintf(stderr, "Missing argument objfile\n");
        exit(1);
    }

    // Open all input files
    int nfiles = 0;
    struct objfile *input_files[argc - optind];

    for (int i = optind; i < argc; ++i) {
        mfile *file;

        int status = mfile_init(&file, argv[i]);
        if (status != 0) {
            fprintf(stderr, "%s: Unable to open file\n", argv[i]);
            mfile_put(file);
            goto invalid_input_file;
        }

        input_files[nfiles] = objfile_load(file, NULL);
        if (input_files[nfiles] == NULL) {
            fprintf(stderr, "%s: Unrecognized format\n", argv[i]);
            mfile_put(file);
            goto invalid_input_file;
        }

        mfile_put(file);
        nfiles++;
    }

    for (int i = 0; i < nfiles; ++i) {
        objfile_put(input_files[i]);
    }

    exit(0);

invalid_input_file:
    for (int i = 0; i < nfiles; ++i) {
        objfile_put(input_files[i]);
    }
    exit(2);
}
