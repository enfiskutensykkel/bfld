#include "image.h"
#include "mfile.h"
#include "objfile.h"
#include <utils/list.h>
#include <utils/rbtree.h>
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
    struct image *image = NULL;

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
    mfile *files[argc - optind + 1];

    for (int i = optind; i < argc; ++i) {

        int status = mfile_init(&files[nfiles], argv[i]);

        if (status != 0) {
            for (int j = 0; j < nfiles; ++j) {
                mfile_put(files[j]);
            }
            exit(2);
        }
        ++nfiles;
    }

    // Parse object files
    struct list_head objfiles = LIST_HEAD_INIT(objfiles);
        
    for (int i = 0; i < nfiles; ++i) {
        fprintf(stderr, "Parsing file: %s\n", argv[optind + i]);

        int status = objfile_parse(&objfiles, files[i]);
        if (status != 0) {
            objfile_list_for_each(objfile, &objfiles) {
                objfile_put(objfile);
            }
            for (int j = 0; j < nfiles; ++j) {
                mfile_put(files[j]);
            }
            exit(2);
        }
    }

    // Build a symbol table
    struct rb_tree symtab = RB_TREE;
    objfile_list_for_each(objfile, &objfiles) {
        objfile_extract_symbols(objfile, &symtab);
    }

    for (; nfiles > 0; --nfiles) {
        mfile_put(files[nfiles-1]);
    }
    objfile_list_for_each(objfile, &objfiles) {
        objfile_put(objfile);
    }

    exit(0);
}
